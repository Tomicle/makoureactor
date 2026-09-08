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
#pragma once

#include <QtCore>

class Section1File;
class Script;
class Opcode;

/**
 * Applies a list of JSON-described edit operations to the scripts of one field.
 *
 * Supported operations (all indices are opcode positions as listed by the
 * `census --scripts` output; negative values count from the end, -1 = last):
 *
 *   {"op":"delete",  "entity":"cl",  "script":4, "from":7, "to":-2}
 *   {"op":"insert",  "entity":"dir", "script":1, "at":0,   "hex":"24 14 00"}
 *   {"op":"insert",  "entity":"dir", "script":1, "at":1,   "copy":{"entity":"cl","script":4,"index":5}}
 *   {"op":"replace", "entity":"cl",  "script":4, "index":0, "hex":"..."}
 *
 * "entity" accepts the group name or its numeric id.
 */
class ScriptEdit
{
public:
	explicit ScriptEdit(Section1File *section1);

	bool apply(const QJsonArray &ops, QString &error);
	/** Entities/scripts touched by apply(), as "entity:script" keys. */
	const QSet<QPair<int, int>> &touched() const { return _touched; }
	QString describe(int groupID, int scriptID) const;
private:
	bool resolveGroup(const QJsonValue &entity, int &groupID, QString &error) const;
	bool resolveScript(const QJsonObject &op, int groupID, int &scriptID, QString &error) const;
	bool resolveIndex(const QJsonValue &value, const Script &script, bool allowEnd, int &index, QString &error) const;
	bool makeOpcode(const QJsonObject &op, Opcode &out, QString &error) const;

	Section1File *_section1;
	QSet<QPair<int, int>> _touched;
};
