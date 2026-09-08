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
#include "ArgumentsCensus.h"

ArgumentsCensus::ArgumentsCensus() : CommonArguments()
{
	_ADD_ARGUMENT(_OPTION_NAMES("o", "output"),
	              "Write the JSON report to this file instead of standard output.", "output", "");
	_ADD_FLAG("no-occurrences", "Omit the per-opcode occurrence list (entity/script/index) and keep only counts.");
	_ADD_FLAG("pretty", "Indent the JSON output.");

	parse();
}

QString ArgumentsCensus::output() const
{
	return _parser.value("output");
}

bool ArgumentsCensus::occurrences() const
{
	return !_parser.isSet("no-occurrences");
}

bool ArgumentsCensus::pretty() const
{
	return _parser.isSet("pretty");
}

void ArgumentsCensus::parse()
{
	_parser.process(*qApp);

	if (_parser.positionalArguments().size() > 2) {
		qWarning() << qPrintable(
		    QCoreApplication::translate("Arguments", "Error: too much parameters"));
		exit(1);
	}

	QStringList paths = wilcardParse();
	if (!paths.isEmpty()) {
		_path = paths.first();
	}
	mapNamesFromFiles();
}
