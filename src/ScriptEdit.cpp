/****************************************************************************
 ** Makou Reactor Final Fantasy VII Field Script Editor
 ** Copyright (C) 2009-2026 Arzel Jérôme <myst6re@gmail.com>
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "ScriptEdit.h"
#include "core/field/Section1File.h"
#include "core/field/FieldPC.h"
#include "core/field/FieldModelLoaderPC.h"

ScriptEdit::ScriptEdit(Section1File *section1) :
    _section1(section1)
{
}

bool ScriptEdit::resolveGroup(const QJsonValue &entity, int &groupID, QString &error) const
{
	const QList<GrpScript> &groups = _section1->grpScripts();

	if (entity.isDouble()) {
		groupID = entity.toInt();
		if (groupID < 0 || groupID >= groups.size()) {
			error = QString("entity id %1 out of range (0..%2)").arg(groupID).arg(groups.size() - 1);
			return false;
		}
		return true;
	}

	QString name = entity.toString();
	for (int i = 0; i < groups.size(); ++i) {
		if (groups.at(i).realName() == name) {
			groupID = i;
			return true;
		}
	}
	error = QString("entity '%1' not found").arg(name);
	return false;
}

bool ScriptEdit::resolveScript(const QJsonObject &op, int groupID, int &scriptID, QString &error) const
{
	if (!op.contains("script")) {
		error = "missing \"script\"";
		return false;
	}
	scriptID = op.value("script").toInt(-1);
	const GrpScript &group = _section1->grpScript(groupID);
	if (scriptID < 0 || scriptID >= group.scripts().size()) {
		error = QString("script %1 out of range (0..%2)").arg(scriptID).arg(group.scripts().size() - 1);
		return false;
	}
	return true;
}

bool ScriptEdit::resolveIndex(const QJsonValue &value, const Script &script, bool allowEnd, int &index, QString &error) const
{
	if (!value.isDouble()) {
		error = "opcode index must be a number";
		return false;
	}
	qsizetype count = script.size();
	index = value.toInt();
	if (index < 0) {
		index = int(count) + index;
	}
	qsizetype max = allowEnd ? count : count - 1;
	if (index < 0 || index > max) {
		error = QString("opcode index %1 out of range (script has %2 opcodes)").arg(value.toInt()).arg(count);
		return false;
	}
	return true;
}

bool ScriptEdit::makeOpcode(const QJsonObject &op, Opcode &out, QString &error) const
{
	if (op.contains("label")) {
		// Pseudo-opcode: jump target. Emits no bytes; jumps referencing it are resolved at compile time.
		OpcodeLABEL label;
		label._label = quint16(op.value("label").toInt());
		out = Opcode(label);
		return true;
	}

	if (op.contains("goto")) {
		// Unconditional jump to a label declared with "label". Direction/size are fixed up by compile().
		quint16 target = quint16(op.value("goto").toInt());
		bool backward = op.value("backward").toBool(true);
		if (backward) {
			OpcodeJMPB jump;
			jump.jump = 0;
			jump._label = target;
			jump._badJump = 0;
			out = Opcode(jump);
		} else {
			OpcodeJMPF jump;
			jump.jump = 0;
			jump._label = target;
			jump._badJump = 0;
			out = Opcode(jump);
		}
		return true;
	}

	if (op.contains("hex")) {
		QString hex = op.value("hex").toString().simplified().remove(' ');
		QByteArray bytes = QByteArray::fromHex(hex.toLatin1());
		if (bytes.isEmpty()) {
			error = "\"hex\" is empty or invalid";
			return false;
		}
		out = Opcode(bytes.constData(), bytes.size());
		if (out.size() != bytes.size()) {
			error = QString("\"hex\" has %1 bytes but opcode %2 expects %3").arg(bytes.size()).arg(out.name()).arg(out.size());
			return false;
		}
		// Conditional / jump opcodes built from hex: bind them to a label so compile() computes the offset.
		if (op.contains("jumpTo")) {
			if (!out.isJump()) {
				error = QString("\"jumpTo\" given but opcode %1 is not a jump/if").arg(out.name());
				return false;
			}
			out.setLabel(quint16(op.value("jumpTo").toInt()));
			out.setBadJump(BadJumpError::Ok);
		}
		return true;
	}

	if (op.contains("copy")) {
		QJsonObject src = op.value("copy").toObject();
		int groupID, scriptID, index;
		if (!resolveGroup(src.value("entity"), groupID, error)) {
			return false;
		}
		if (!resolveScript(src, groupID, scriptID, error)) {
			return false;
		}
		const Script &script = _section1->grpScript(groupID).script(quint8(scriptID));
		if (!resolveIndex(src.value("index"), script, false, index, error)) {
			return false;
		}
		out = script.opcode(index);
		return true;
	}

	error = "insert/replace needs \"hex\" or \"copy\"";
	return false;
}

bool ScriptEdit::apply(const QJsonArray &ops, QString &error)
{
	int n = 0;
	for (const QJsonValue &value : ops) {
		++n;
		QJsonObject op = value.toObject();
		QString kind = op.value("op").toString();
		QString err;

		if (kind == "add-entity") {
			// Append a new non-model entity with empty Init/Main (each just RET).
			QString name = op.value("name").toString();
			if (name.isEmpty() || name.size() > 8) {
				error = QString("op #%1 (add-entity): name must be 1..8 characters").arg(n);
				return false;
			}
			for (const GrpScript &g : _section1->grpScripts()) {
				if (g.realName() == name) {
					error = QString("op #%1 (add-entity): entity '%2' already exists").arg(n).arg(name);
					return false;
				}
			}
			const char ret = 0;
			Script retScript(QList<Opcode>{ Opcode(&ret, 1) });
			GrpScript group(name);
			group.setScript(0, retScript);
			group.setScript(1, retScript);
			int row = int(_section1->grpScriptCount());
			if (!_section1->insertGrpScript(row, group)) {
				error = QString("op #%1 (add-entity): entity limit reached").arg(n);
				return false;
			}
			_touched.insert(qMakePair(row, 0));
			continue;
		}

		if (kind == "add-character") {
			// Add a playable-character field model (Section 3) and a CHAR+PC entity for it (Section 1).
			// {"op":"add-character","name":"pftifa","charId":2,"hrc":"AAGB","anims":["ABCD","ABCE","ABCF"],"charName":"..."}
			QString name = op.value("name").toString();
			int charId = op.value("charId").toInt(-1);
			QString hrc = op.value("hrc").toString().toUpper();
			QStringList anims;
			for (const QJsonValue &a : op.value("anims").toArray()) {
				anims.append(a.toString());
			}
			if (name.isEmpty() || name.size() > 8 || charId < 0 || hrc.isEmpty() || anims.isEmpty()) {
				error = QString("op #%1 (add-character): need name (1..8), charId, hrc, anims").arg(n);
				return false;
			}
			Field *field = _section1->field();
			if (!field->isPC()) {
				error = QString("op #%1 (add-character): PC fields only").arg(n);
				return false;
			}
			FieldModelLoaderPC *ml = static_cast<FieldPC *>(field)->fieldModelLoader();
			if (ml == nullptr || !ml->isOpen() || ml->modelCount() == 0) {
				error = QString("op #%1 (add-character): model loader unavailable or empty").arg(n);
				return false;
			}
			for (const GrpScript &g : _section1->grpScripts()) {
				if (g.realName() == name) {
					error = QString("op #%1 (add-character): entity '%2' already exists").arg(n).arg(name);
					return false;
				}
			}
			// Template: the first model in the field (lighting, scale flags); override identity and animations.
			FieldModelInfosPC info = ml->modelInfos(0);
			info.nameChar = op.contains("charName") ? op.value("charName").toString()
			                                        : QString("%1_%2.char").arg(field->name(), name);
			info.nameHRC = hrc.endsWith(".HRC") ? hrc : hrc + ".HRC";
			info.anims.clear();
			info.animsUnknown.clear();
			quint16 animUnknown = info.animsUnknown.isEmpty() ? 1 : info.animsUnknown.first();
			for (const QString &a : anims) {
				info.anims.append(a);                 // e.g. "ACFE.aki" (as listed in the model loader)
				info.animsUnknown.append(animUnknown);
			}
			int modelID = int(ml->modelCount());
			ml->insertModelInfos(modelID, info);

			// Entity: Init = CHAR modelID ; PC charId ; RET   Main = RET
			const char initBytes[] = { char(0xA1), char(modelID), char(0xA0), char(charId), 0 };
			QList<Opcode> initOps;
			initOps.append(Opcode(initBytes, 2));
			initOps.append(Opcode(initBytes + 2, 2));
			initOps.append(Opcode(initBytes + 4, 1));
			const char ret = 0;
			GrpScript group(name);
			group.setScript(0, Script(initOps));
			group.setScript(1, Script(QList<Opcode>{ Opcode(&ret, 1) }));
			int row = int(_section1->grpScriptCount());
			if (!_section1->insertGrpScript(row, group)) {
				error = QString("op #%1 (add-character): entity limit reached").arg(n);
				return false;
			}
			_touched.insert(qMakePair(row, 0));
			continue;
		}

		int groupID, scriptID;
		if (!resolveGroup(op.value("entity"), groupID, err) || !resolveScript(op, groupID, scriptID, err)) {
			error = QString("op #%1: %2").arg(n).arg(err);
			return false;
		}
		Script &script = _section1->grpScript(groupID).script(quint8(scriptID));

		if (kind == "delete") {
			int from, to;
			if (!resolveIndex(op.value("from"), script, false, from, err)) {
				error = QString("op #%1 (delete): %2").arg(n).arg(err);
				return false;
			}
			if (op.contains("to")) {
				if (!resolveIndex(op.value("to"), script, false, to, err)) {
					error = QString("op #%1 (delete): %2").arg(n).arg(err);
					return false;
				}
			} else {
				to = from;
			}
			if (to < from) {
				error = QString("op #%1 (delete): \"to\" (%2) is before \"from\" (%3)").arg(n).arg(to).arg(from);
				return false;
			}
			for (int i = to; i >= from; --i) {
				script.removeOpcode(i);
			}
		} else if (kind == "insert") {
			int at;
			if (!resolveIndex(op.value("at"), script, true, at, err)) {
				error = QString("op #%1 (insert): %2").arg(n).arg(err);
				return false;
			}
			Opcode opcode;
			if (!makeOpcode(op, opcode, err)) {
				error = QString("op #%1 (insert): %2").arg(n).arg(err);
				return false;
			}
			script.insertOpcode(at, opcode);
		} else if (kind == "replace") {
			int index;
			if (!resolveIndex(op.value("index"), script, false, index, err)) {
				error = QString("op #%1 (replace): %2").arg(n).arg(err);
				return false;
			}
			Opcode opcode;
			if (!makeOpcode(op, opcode, err)) {
				error = QString("op #%1 (replace): %2").arg(n).arg(err);
				return false;
			}
			script.setOpcode(index, opcode);
		} else {
			error = QString("op #%1: unknown op \"%2\" (expected delete, insert or replace)").arg(n).arg(kind);
			return false;
		}

		_touched.insert(qMakePair(groupID, scriptID));
	}

	return true;
}

QString ScriptEdit::describe(int groupID, int scriptID) const
{
	const GrpScript &group = _section1->grpScript(groupID);
	const Script &script = group.script(quint8(scriptID));
	QString out = QString("== %1 (entity %2) / %3 (script %4) ==\n")
	                  .arg(group.realName()).arg(groupID).arg(group.scriptName(quint8(scriptID))).arg(scriptID);
	int i = 0;
	for (const Opcode &opcode : script.opcodes()) {
		out += QString("%1  %2\n").arg(i++, 3).arg(opcode.toString(_section1));
	}
	return out;
}
