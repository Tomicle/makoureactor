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
#include "Census.h"
#include "core/field/FieldArchive.h"
#include "core/field/Field.h"
#include "core/field/FieldPC.h"
#include "core/field/FieldModelLoaderPC.h"
#include "core/field/Section1File.h"
#include "core/field/InfFile.h"
#include "core/field/IdFile.h"

// Playable character IDs as used by the PC opcode (0 Cloud .. 8 Cid)
static const int PLAYABLE_CHARACTER_COUNT = 9;

const QList<OpcodeKey> &Census::trackedOpcodes()
{
	static const QList<OpcodeKey> keys = {
		// Party composition and split/join
		OpcodeKey::JOIN, OpcodeKey::SPLIT, OpcodeKey::SPTYE, OpcodeKey::GTPYE,
		OpcodeKey::GETPC, OpcodeKey::PXYZI,
		OpcodeKey::PRTYP, OpcodeKey::PRTYM, OpcodeKey::PRTYE,
		OpcodeKey::IFPRTYQ, OpcodeKey::IFMEMBQ,
		OpcodeKey::MMBud, OpcodeKey::MMBLK, OpcodeKey::MMBUK,
		// Entity definition and control
		OpcodeKey::PC, OpcodeKey::CHAR_, OpcodeKey::CC, OpcodeKey::UC,
		OpcodeKey::PDIRA, OpcodeKey::PTURA, OpcodeKey::PMOVA,
		// Movement / placement
		OpcodeKey::MOVE, OpcodeKey::CMOVE, OpcodeKey::MOVA, OpcodeKey::FMOVE,
		OpcodeKey::XYZI, OpcodeKey::XYI, OpcodeKey::XYZ, OpcodeKey::AXYZI,
		OpcodeKey::JUMP, OpcodeKey::LADER, OpcodeKey::OFST, OpcodeKey::OFSTW,
		OpcodeKey::MSPED, OpcodeKey::ASPED,
		OpcodeKey::GETAI, OpcodeKey::GETAXY, OpcodeKey::GETDIR,
		// Visibility / collision / interaction radii
		OpcodeKey::VISI, OpcodeKey::SOLID, OpcodeKey::TALKR, OpcodeKey::SLIDR,
		OpcodeKey::TLKR2, OpcodeKey::SLDR2, OpcodeKey::LINE, OpcodeKey::LINON,
		// Field lifecycle
		OpcodeKey::MAPJUMP, OpcodeKey::MINIGAME, OpcodeKey::BATTLE, OpcodeKey::MENU,
		OpcodeKey::PMJMP, OpcodeKey::PMJMP2
	};
	return keys;
}

const QList<OpcodeKey> &Census::detailedOpcodes()
{
	static const QList<OpcodeKey> keys = {
		OpcodeKey::JOIN, OpcodeKey::SPLIT, OpcodeKey::SPTYE, OpcodeKey::GTPYE,
		OpcodeKey::GETPC, OpcodeKey::PXYZI,
		OpcodeKey::PRTYP, OpcodeKey::PRTYM, OpcodeKey::PRTYE,
		OpcodeKey::IFPRTYQ, OpcodeKey::IFMEMBQ,
		OpcodeKey::MMBud, OpcodeKey::MMBLK, OpcodeKey::MMBUK,
		OpcodeKey::PC, OpcodeKey::CC, OpcodeKey::UC,
		OpcodeKey::PDIRA, OpcodeKey::PTURA, OpcodeKey::PMOVA,
		OpcodeKey::SOLID, OpcodeKey::MAPJUMP, OpcodeKey::MINIGAME
	};
	return keys;
}

Census::Census(FieldArchive *archive, bool withOccurrences, bool withScripts) :
    _archive(archive), _withOccurrences(withOccurrences), _withScripts(withScripts), _fieldsWithAllPlayable(0), _fieldsWithGateways(0)
{
}

QJsonObject Census::opcodeDetail(const Opcode &opcode, const QList<QString> &entityNames)
{
	QJsonObject detail;
	const Opcode::OPCODE &op = opcode.op();

	switch (opcode.id()) {
	case OpcodeKey::PC:
	case OpcodeKey::PRTYP:
	case OpcodeKey::PRTYM:
	case OpcodeKey::MMBLK:
	case OpcodeKey::MMBUK:
		detail["charId"] = op.opcodePC.charID;
		detail["character"] = Opcode::character(op.opcodePC.charID);
		break;
	case OpcodeKey::IFPRTYQ:
	case OpcodeKey::IFMEMBQ:
		detail["charId"] = op.opcodeIFPRTYQ.charID;
		detail["character"] = Opcode::character(op.opcodeIFPRTYQ.charID);
		break;
	case OpcodeKey::MMBud:
		detail["charId"] = op.opcodeMMBud.charID;
		detail["character"] = Opcode::character(op.opcodeMMBud.charID);
		detail["exists"] = op.opcodeMMBud.exists;
		break;
	case OpcodeKey::PRTYE: {
		QJsonArray chars;
		for (int i = 0; i < 3; ++i) {
			chars.append(op.opcodePRTYE.charID[i]);
		}
		detail["charIds"] = chars;
		break;
	}
	case OpcodeKey::SPLIT:
		detail["speed"] = op.opcodeSPLIT.speed;
		detail["banks"] = (op.opcodeSPLIT.banks[0] | op.opcodeSPLIT.banks[1] | op.opcodeSPLIT.banks[2]) != 0;
		break;
	case OpcodeKey::JOIN:
		detail["speed"] = op.opcodeJOIN.speed;
		break;
	case OpcodeKey::CC: {
		int group = op.opcodeCC.groupID;
		detail["groupId"] = group;
		if (group >= 0 && group < entityNames.size()) {
			detail["entity"] = entityNames.at(group);
		}
		break;
	}
	case OpcodeKey::UC:
		detail["disabled"] = op.opcodeUC.disabled;
		break;
	case OpcodeKey::SOLID:
		detail["disabled"] = op.opcodeSOLID.disabled;
		break;
	case OpcodeKey::PXYZI:
		detail["partyId"] = op.opcodePXYZI.partyID;
		break;
	case OpcodeKey::GETPC:
		detail["partyId"] = op.opcodeGETPC.partyID;
		break;
	case OpcodeKey::PDIRA:
	case OpcodeKey::PTURA:
	case OpcodeKey::PMOVA:
		detail["partyId"] = op.opcodePMOVA.partyID;
		break;
	case OpcodeKey::MAPJUMP:
		detail["mapId"] = op.opcodeMAPJUMP.mapID;
		break;
	case OpcodeKey::MINIGAME:
		detail["minigameId"] = op.opcodeMINIGAME.minigameID;
		detail["mapId"] = op.opcodeMINIGAME.mapID;
		break;
	default:
		break;
	}

	return detail;
}

QJsonObject Census::fieldReport(int mapID, Field *field)
{
	QJsonObject report;
	report["mapId"] = mapID;
	report["name"] = field->name();

	Section1File *section1 = field->scriptsAndTexts();
	if (section1 == nullptr || !section1->isOpen()) {
		report["error"] = "section 1 not readable";
		return report;
	}

	FieldModelLoaderPC *modelLoader = nullptr;
	if (field->isPC()) {
		FieldModelLoaderPC *loader = static_cast<FieldPC *>(field)->fieldModelLoader();
		if (loader != nullptr && loader->isOpen()) {
			modelLoader = loader;
		}
	}

	const QList<GrpScript> &groups = section1->grpScripts();
	QList<QString> entityNames;
	entityNames.reserve(groups.size());
	for (const GrpScript &group : groups) {
		entityNames.append(group.realName());
	}

	QJsonArray entities;
	QSet<int> pcCharacters;
	QMap<QString, int> opcodeCounts;
	QJsonArray occurrences;
	int totalOpcodes = 0;
	qsizetype totalScriptBytes = 0;

	const QList<OpcodeKey> &tracked = trackedOpcodes();
	const QList<OpcodeKey> &detailed = detailedOpcodes();

	for (int groupID = 0; groupID < groups.size(); ++groupID) {
		const GrpScript &group = groups.at(groupID);
		QJsonObject entity;
		entity["id"] = groupID;
		entity["name"] = group.realName();
		entity["type"] = group.typeString();

		if (group.type() == GrpScript::Model) {
			int charID = group.character();
			if (charID >= 0 && charID < 0x100) {
				entity["charId"] = charID;
				entity["character"] = Opcode::character(quint8(charID));
				pcCharacters.insert(charID);
			}
			int modelID = section1->modelID(quint8(groupID));
			if (modelID >= 0) {
				entity["modelId"] = modelID;
				if (modelLoader != nullptr && modelID < modelLoader->modelCount()) {
					entity["hrc"] = modelLoader->HRCName(modelID);
					entity["modelName"] = modelLoader->charName(modelID);
				}
			}
		}

		QJsonArray scriptSizes;
		QJsonArray scriptTexts;
		int entityOpcodes = 0;
		qsizetype entityBytes = 0;
		const QVarLengthArray<Script> &scripts = group.scripts();
		for (int scriptID = 0; scriptID < scripts.size(); ++scriptID) {
			const Script &script = scripts.at(scriptID);
			qsizetype bytes = script.toByteArray().size();
			scriptSizes.append(int(bytes));
			entityBytes += bytes;
			if (_withScripts) {
				QJsonObject text;
				text["id"] = scriptID;
				text["name"] = group.scriptName(quint8(scriptID));
				text["text"] = script.isEmpty() ? QString() : script.toString(section1);
				scriptTexts.append(text);
			}

			int opcodeID = 0;
			for (const Opcode &opcode : script.opcodes()) {
				OpcodeKey key = opcode.id();
				if (key < 257) {
					++entityOpcodes;
				}
				if (tracked.contains(key)) {
					opcodeCounts[QString::fromLatin1(opcode.name())] += 1;
					if (_withOccurrences && detailed.contains(key)) {
						QJsonObject occurrence;
						occurrence["op"] = QString::fromLatin1(opcode.name());
						occurrence["entityId"] = groupID;
						occurrence["entity"] = group.realName();
						occurrence["script"] = scriptID;
						occurrence["index"] = opcodeID;
						QJsonObject detail = opcodeDetail(opcode, entityNames);
						if (!detail.isEmpty()) {
							occurrence["detail"] = detail;
						}
						occurrences.append(occurrence);
					}
				}
				++opcodeID;
			}
		}
		entity["scriptBytes"] = scriptSizes;
		if (_withScripts) {
			entity["scripts"] = scriptTexts;
		}
		entity["opcodeCount"] = entityOpcodes;
		totalOpcodes += entityOpcodes;
		totalScriptBytes += entityBytes;
		entities.append(entity);
	}

	// availableBytesForScripts() is the total capacity of the script area
	// (65535 minus header and entity table), not what is left.
	qsizetype capacity = section1->availableBytesForScripts();
	report["scriptBytesUsed"] = int(totalScriptBytes);
	report["scriptBytesCapacity"] = int(capacity);
	report["scriptBytesRemaining"] = int(capacity - totalScriptBytes);
	report["entityCount"] = int(groups.size());
	report["opcodeCount"] = totalOpcodes;
	report["textCount"] = int(section1->textCount());
	report["entities"] = entities;

	QList<int> pcList = pcCharacters.values();
	std::sort(pcList.begin(), pcList.end());
	QJsonArray pcArray;
	for (int c : pcList) {
		pcArray.append(c);
		_fieldsWithPcCharacter[c] += 1;
	}
	report["pcCharacters"] = pcArray;

	bool allPlayable = true;
	for (int c = 0; c < PLAYABLE_CHARACTER_COUNT; ++c) {
		if (!pcCharacters.contains(c)) {
			allPlayable = false;
			break;
		}
	}
	report["hasAllPlayablePc"] = allPlayable;
	if (allPlayable) {
		++_fieldsWithAllPlayable;
	}

	// Gateways (Section 8 "inf"): exit lines with destination field and spawn position
	InfFile *inf = field->inf();
	if (inf != nullptr && inf->isOpen()) {
		QJsonArray gateways;
		int id = 0;
		for (const Exit &exit : inf->exitLines()) {
			if (exit.fieldID != 0x7FFF) {
				QJsonObject g;
				g["id"] = id;
				g["toMapId"] = exit.fieldID;
				g["toMap"] = _archive->mapName(exit.fieldID);
				g["destX"] = exit.destination.x;
				g["destY"] = exit.destination.y;
				g["destTriangle"] = exit.destination.z;
				g["destDir"] = exit.dir;
				QJsonArray line;
				for (int i = 0; i < 2; ++i) {
					QJsonArray v;
					v.append(exit.exit_line[i].x);
					v.append(exit.exit_line[i].y);
					v.append(exit.exit_line[i].z);
					line.append(v);
				}
				g["line"] = line;
				gateways.append(g);
			}
			++id;
		}
		report["gateways"] = gateways;
		report["gatewayCount"] = gateways.size();
		_fieldsWithGateways += gateways.isEmpty() ? 0 : 1;
	}

	// Walkmesh (Section 5): triangle vertices + centroids, only with --scripts (large)
	if (_withScripts) {
		IdFile *walkmesh = field->walkmesh();
		if (walkmesh != nullptr && walkmesh->isOpen()) {
			QJsonArray tris;
			int id = 0;
			for (const Triangle &t : walkmesh->triangles()) {
				QJsonObject tri;
				tri["id"] = id;
				QJsonArray verts;
				int cx = 0, cy = 0, cz = 0;
				for (int i = 0; i < 3; ++i) {
					QJsonArray v;
					v.append(t.vertices[i].x);
					v.append(t.vertices[i].y);
					v.append(t.vertices[i].z);
					verts.append(v);
					cx += t.vertices[i].x;
					cy += t.vertices[i].y;
					cz += t.vertices[i].z;
				}
				tri["v"] = verts;
				QJsonArray c;
				c.append(cx / 3);
				c.append(cy / 3);
				c.append(cz / 3);
				tri["centroid"] = c;
				const Access &acc = walkmesh->access(id);
				QJsonArray adj;
				for (int i = 0; i < 3; ++i) {
					adj.append(acc.a[i]);
				}
				tri["adjacent"] = adj;
				tris.append(tri);
				++id;
			}
			report["walkmesh"] = tris;
		}
	}

	if (modelLoader != nullptr) {
		QJsonArray models;
		for (int i = 0; i < modelLoader->modelCount(); ++i) {
			models.append(modelLoader->HRCName(i));
		}
		report["models"] = models;
	}

	QJsonObject counts;
	for (auto it = opcodeCounts.constBegin(); it != opcodeCounts.constEnd(); ++it) {
		counts[it.key()] = it.value();
		_fieldsWithOpcode[it.key()] += 1;
	}
	report["opcodes"] = counts;

	if (_withOccurrences) {
		report["occurrences"] = occurrences;
	}

	return report;
}

QJsonObject Census::run(const QList<int> &mapIDs)
{
	_fieldsWithOpcode.clear();
	_fieldsWithPcCharacter.clear();
	_fieldsWithAllPlayable = 0;
	_fieldsWithGateways = 0;

	QJsonArray fields;
	int errors = 0;

	for (int mapID : mapIDs) {
		Field *field = _archive->field(mapID);
		if (field == nullptr) {
			QJsonObject report;
			report["mapId"] = mapID;
			report["error"] = "field not readable";
			fields.append(report);
			++errors;
			continue;
		}
		QJsonObject report = fieldReport(mapID, field);
		if (report.contains("error")) {
			++errors;
		}
		fields.append(report);
	}

	QJsonObject summary;
	summary["fields"] = int(mapIDs.size());
	summary["errors"] = errors;
	summary["fieldsWithAllPlayablePc"] = _fieldsWithAllPlayable;
	summary["fieldsWithGateways"] = _fieldsWithGateways;

	QJsonObject withOpcode;
	for (auto it = _fieldsWithOpcode.constBegin(); it != _fieldsWithOpcode.constEnd(); ++it) {
		withOpcode[it.key()] = it.value();
	}
	summary["fieldsWithOpcode"] = withOpcode;

	QJsonObject pcCoverage;
	for (auto it = _fieldsWithPcCharacter.constBegin(); it != _fieldsWithPcCharacter.constEnd(); ++it) {
		QJsonObject entry;
		entry["character"] = Opcode::character(quint8(it.key()));
		entry["fields"] = it.value();
		pcCoverage[QString::number(it.key())] = entry;
	}
	summary["fieldsWithPcCharacter"] = pcCoverage;

	QJsonObject root;
	root["generator"] = QString("%1 census").arg(MAKOU_REACTOR_NAME);
	root["version"] = QString(MAKOU_REACTOR_VERSION);
	root["platform"] = _archive->isPC() ? "PC" : "PS";
	root["trackedOpcodes"] = [] {
		QJsonArray a;
		for (OpcodeKey key : trackedOpcodes()) {
			a.append(QString::fromLatin1(Opcode::names[key]));
		}
		return a;
	}();
	root["summary"] = summary;
	root["fields"] = fields;

	return root;
}
