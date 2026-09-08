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
#include "core/field/Opcode.h"

class Field;
class FieldArchive;

/**
 * Read-only survey of field scripts, intended for automated analysis.
 *
 * For every selected field the census records the entity list (with
 * character/model bindings), the opcodes that matter for party handling and
 * entity movement, and the remaining Section 1 script budget. The result is
 * a JSON document that downstream tools can diff, filter and aggregate.
 */
class Census
{
public:
	Census(FieldArchive *archive, bool withOccurrences);

	QJsonObject fieldReport(int mapID, Field *field);
	QJsonObject run(const QList<int> &mapIDs);

	/** Opcodes that are counted per field. */
	static const QList<OpcodeKey> &trackedOpcodes();
	/** Subset of trackedOpcodes() for which individual occurrences are listed. */
	static const QList<OpcodeKey> &detailedOpcodes();
private:
	static QJsonObject opcodeDetail(const Opcode &opcode, const QList<QString> &entityNames);

	FieldArchive *_archive;
	bool _withOccurrences;
	QMap<QString, int> _fieldsWithOpcode;
	QMap<int, int> _fieldsWithPcCharacter;
	int _fieldsWithAllPlayable;
};
