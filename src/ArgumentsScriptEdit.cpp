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
#include "ArgumentsScriptEdit.h"

ArgumentsScriptEdit::ArgumentsScriptEdit() : CommonArguments()
{
	_ADD_ARGUMENT("field", "Name of the field to edit (required).", "field", "");
	_ADD_ARGUMENT("ops", "JSON file describing the edit operations (required). "
	                     "Array of objects with \"op\": delete | insert | replace. "
	                     "Addressing: \"entity\" (name or id), \"script\" (0 = init, 1 = main, ...), "
	                     "\"index\"/\"from\"/\"to\"/\"at\" (opcode positions; negative counts from the end). "
	                     "New opcodes come from \"hex\" (raw bytes) or \"copy\": {entity, script, index}.", "ops", "");
	_ADD_FLAG("dry-run", "Apply the operations in memory and print the affected scripts without saving.");

	_parser.addPositionalArgument(
	    "target_file", QCoreApplication::translate("Arguments", "Output file (optional, defaults to in-place)."), "[target_file]"
	);

	parse();
}

QString ArgumentsScriptEdit::field() const
{
	return _parser.value("field");
}

QString ArgumentsScriptEdit::opsFile() const
{
	return _parser.value("ops");
}

QString ArgumentsScriptEdit::targetFile() const
{
	return _target_file.isEmpty() ? _path : _target_file;
}

bool ArgumentsScriptEdit::dryRun() const
{
	return _parser.isSet("dry-run");
}

void ArgumentsScriptEdit::parse()
{
	_parser.process(*qApp);

	if (_parser.positionalArguments().size() > 3) {
		qWarning() << qPrintable(
		    QCoreApplication::translate("Arguments", "Error: too much parameters"));
		exit(1);
	}

	QStringList paths = wilcardParse();
	if (!paths.isEmpty()) {
		_path = paths.first();
		if (paths.size() > 1) {
			_target_file = paths.at(1);
		}
	}
	mapNamesFromFiles();
}
