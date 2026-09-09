/****************************************************************************
 ** Makou Reactor Final Fantasy VII Field Script Editor
 ** Copyright (C) 2009-2021 Arzel Jérôme <myst6re@gmail.com>
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
#include "CLI.h"
#include "Arguments.h"
#include "ArgumentsExport.h"
#include "ArgumentsPatch.h"
#include "ArgumentsTools.h"
#include "ArgumentsCensus.h"
#include "Census.h"
#include "ArgumentsScriptEdit.h"
#include "ScriptEdit.h"
#include "core/field/Section1File.h"
#include "core/field/FieldArchivePS.h"
#include "core/field/FieldArchivePC.h"
#include "core/field/BackgroundFilePC.h"
#include <iostream>

void CLIObserver::setObserverValue(int value)
{
	quint8 percent = quint8(value * 100.0 / double(_maximum));

	if (percent != _lastPercent) {
		_lastPercent = percent;
		setPercent(percent);
	}
}

void CLIObserver::setPercent(quint8 percent)
{
	printf("[%d%%] %s\r", percent, qPrintable(_filename));
	fflush(stdout);
}

bool CLIObserver::observerRetry(const QString &message)
{
	qInfo() << qPrintable(message);
	std::cout << qPrintable(QCoreApplication::translate("CLI", "Retry? [Yn] ")) << std::flush;
	std::string line;
	std::getline(std::cin, line);
	if (std::cin.eof()) {
		return false;
	}

	QString qtLine = QString::fromStdString(line);

	return qtLine.isEmpty() || qtLine.compare(QCoreApplication::translate("CLI", "y"), Qt::CaseInsensitive) == 0;
}

CLIObserver CLI::observer;

void CLI::commandExport()
{
	ArgumentsExport argsExport;
	if (argsExport.help() || argsExport.destination().isEmpty()) {
		argsExport.showHelp();
	}

	FieldArchive *fieldArchive = openFieldArchive(argsExport.inputFormat(), argsExport.path());
	if (fieldArchive == nullptr) {
		return;
	}

	PsfTags tags = argsExport.psfTags();
	QList<int> selectedFields;
	QList<QRegularExpression> includes, excludes;
	QStringList includePatterns = argsExport.includes(), excludePatterns = argsExport.excludes();

	for (const QString &pattern: includePatterns) {
		includes.append(QRegularExpression(QRegularExpression::anchoredPattern(QRegularExpression::wildcardToRegularExpression(pattern))));
	}
	for (const QString &pattern: excludePatterns) {
		excludes.append(QRegularExpression(QRegularExpression::anchoredPattern(QRegularExpression::wildcardToRegularExpression(pattern))));
	}

	FieldArchiveIterator it(*fieldArchive);
	while (it.hasNext()) {
		const Field *field = it.next(false);
		if (field != nullptr) {
			bool found = includes.isEmpty();
			for (const QRegularExpression &regExp: includes) {
				if (regExp.match(field->name()).hasMatch()) {
					found = true;
					break;
				}
			}
			for (const QRegularExpression &regExp: excludes) {
				if (regExp.match(field->name()).hasMatch()) {
					found = false;
					break;
				}
			}

			if (found) {
				selectedFields.append(it.mapId());
			}
		}
	}

	QMap<FieldArchive::ExportType, QString> toExport;

	if (!argsExport.mapFileFormat().isEmpty()) {
		toExport.insert(FieldArchive::Fields, argsExport.mapFileFormat());
	}
	if (!argsExport.backgroundFormat().isEmpty()) {
		toExport.insert(FieldArchive::Backgrounds, argsExport.backgroundFormat());
	}
	if (!argsExport.soundFormat().isEmpty()) {
		toExport.insert(FieldArchive::Akaos, argsExport.soundFormat());
	}
	if (!argsExport.textFormat().isEmpty()) {
		toExport.insert(FieldArchive::Texts, argsExport.textFormat());
	}
	if (!argsExport.chunkFormat().isEmpty()) {
		toExport.insert(FieldArchive::Chunks, argsExport.chunkFormat());
	}

	if (!fieldArchive->exportation(selectedFields, argsExport.destination(),
								   argsExport.force(), toExport, &tags)) {
		qWarning() << qPrintable(QCoreApplication::translate("CLI", "An error occured when exporting"));
	}

	delete fieldArchive;
}

void CLI::commandPatch()
{
	ArgumentsPatch argsPatch;
	if (argsPatch.help() || argsPatch.path().isEmpty()) {
		argsPatch.showHelp();
	}

	FieldArchive *fieldArchive = openFieldArchive(argsPatch.inputFormat(), argsPatch.path());
	if (fieldArchive == nullptr) {
		return;
	}

	QList<int> selectedFields;
	QList<QRegularExpression> includes, excludes;
	QStringList includePatterns = argsPatch.includes(), excludePatterns = argsPatch.excludes();

	for (const QString &pattern: includePatterns) {
		includes.append(QRegularExpression(QRegularExpression::anchoredPattern(QRegularExpression::wildcardToRegularExpression(pattern))));
	}
	for (const QString &pattern: excludePatterns) {
		excludes.append(QRegularExpression(QRegularExpression::anchoredPattern(QRegularExpression::wildcardToRegularExpression(pattern))));
	}

	FieldArchiveIterator it(*fieldArchive);
	while (it.hasNext()) {
		const Field *field = it.next(false);
		if (field != nullptr) {
			bool found = includes.isEmpty();
			for (const QRegularExpression &regExp: includes) {
				if (regExp.match(field->name()).hasMatch()) {
					found = true;
					break;
				}
			}
			for (const QRegularExpression &regExp: excludes) {
				if (regExp.match(field->name()).hasMatch()) {
					found = false;
					break;
				}
			}

			if (found) {
				selectedFields.append(it.mapId());
			}
		}
	}

	observer.setObserverMaximum(uint(selectedFields.size()));

	int i = 0;

	for (const int &mapID : selectedFields) {
		Field *field = fieldArchive->field(mapID);
		if (field != nullptr) {
			if (argsPatch.removeDialogs() && field->scriptsAndTexts()->isOpen()) {
				field->scriptsAndTexts()->removeTexts();
				if (field->scriptsAndTexts()->isModified() && !field->isModified()) {
					field->setModified(true);
				}
			}

			if (argsPatch.emptyUnusedTexts() && field->scriptsAndTexts()->isOpen()) {
				field->scriptsAndTexts()->cleanTexts();
				if (field->scriptsAndTexts()->isModified() && !field->isModified()) {
					field->setModified(true);
				}
			}

			if (argsPatch.removeEncounters() && field->encounter()->isOpen()) {
				field->encounter()->setBattleEnabled(EncounterFile::Table1, false);
				field->encounter()->setBattleEnabled(EncounterFile::Table2, false);
				if (field->encounter()->isModified() && !field->isModified()) {
					field->setModified(true);
				}
			}

			if (argsPatch.autosizeTextWindows() && field->scriptsAndTexts()->isOpen()) {
				field->scriptsAndTexts()->autosizeTextWindows();
				if (field->scriptsAndTexts()->isModified() && !field->isModified()) {
					field->setModified(true);
				}
			}

			if (fieldArchive->isPC()) {
				if (argsPatch.cleanModelLoader()) {
					FieldPC *fieldPC = static_cast<FieldPC *>(field);
					FieldModelLoaderPC *modelLoader = fieldPC->fieldModelLoader();
					if (modelLoader->isOpen()) {
						modelLoader->clean();
						if (modelLoader->isModified() && !field->isModified()) {
							field->setModified(true);
						}
					}
				}

				if (argsPatch.repairBackgrounds()
				    && (field->name().toLower() == "lastmap"
				        || field->name().toLower() == "fr_e")) {
					BackgroundFilePC *bg = static_cast<BackgroundFilePC *>(field->background());
					if (bg->isOpen() && bg->repair()) {
						field->setModified(true);
					}
				}

				if (argsPatch.removeTilesSections()) {
					FieldPC *fieldPC = static_cast<FieldPC *>(field);
					fieldPC->setRemoveUnusedSection(true);
					field->setModified(true);
				}
			}
		}

		observer.setObserverValue(i++);
	}

	fieldArchive->save(argsPatch.targetFile());

	delete fieldArchive;
}

void CLI::commandTools()
{
	ArgumentsTools argsTools;
	if (argsTools.help() || argsTools.path().isEmpty()) {
		argsTools.showHelp();
	}

	FieldArchive *fieldArchive = openFieldArchive(argsTools.inputFormat(), argsTools.path());
	if (fieldArchive == nullptr) {
		return;
	}

	QList<int> selectedFields;
	QList<QRegularExpression> includes, excludes;
	QStringList includePatterns = argsTools.includes(), excludePatterns = argsTools.excludes();

	for (const QString &pattern: includePatterns) {
		includes.append(QRegularExpression(QRegularExpression::anchoredPattern(QRegularExpression::wildcardToRegularExpression(pattern))));
	}
	for (const QString &pattern: excludePatterns) {
		excludes.append(QRegularExpression(QRegularExpression::anchoredPattern(QRegularExpression::wildcardToRegularExpression(pattern))));
	}

	FieldArchiveIterator it(*fieldArchive);
	while (it.hasNext()) {
		const Field *field = it.next(false);
		if (field != nullptr) {
			bool found = includes.isEmpty();
			for (const QRegularExpression &regExp: includes) {
				if (regExp.match(field->name()).hasMatch()) {
					found = true;
					break;
				}
			}
			for (const QRegularExpression &regExp: excludes) {
				if (regExp.match(field->name()).hasMatch()) {
					found = false;
					break;
				}
			}

			if (found) {
				selectedFields.append(it.mapId());
			}
		}
	}

	observer.setObserverMaximum(uint(selectedFields.size()));

	int i = 0;

	for (const int &mapID : selectedFields) {
		Field *field = fieldArchive->field(mapID);
		if (field != nullptr) {
			if (fieldArchive->isPC()) {
				BackgroundFilePC *bg = static_cast<BackgroundFilePC *>(field->background());
				if (bg->isOpen()) {
					bg->untile(argsTools.dir());
				}
			}
		}

		observer.setObserverValue(i++);
	}

	delete fieldArchive;
}

QList<int> CLI::selectFields(FieldArchive *fieldArchive, const QStringList &includePatterns, const QStringList &excludePatterns)
{
	QList<int> selectedFields;
	QList<QRegularExpression> includes, excludes;

	for (const QString &pattern: includePatterns) {
		includes.append(QRegularExpression(QRegularExpression::anchoredPattern(QRegularExpression::wildcardToRegularExpression(pattern))));
	}
	for (const QString &pattern: excludePatterns) {
		excludes.append(QRegularExpression(QRegularExpression::anchoredPattern(QRegularExpression::wildcardToRegularExpression(pattern))));
	}

	FieldArchiveIterator it(*fieldArchive);
	while (it.hasNext()) {
		const Field *field = it.next(false);
		if (field != nullptr) {
			bool found = includes.isEmpty();
			for (const QRegularExpression &regExp: includes) {
				if (regExp.match(field->name()).hasMatch()) {
					found = true;
					break;
				}
			}
			for (const QRegularExpression &regExp: excludes) {
				if (regExp.match(field->name()).hasMatch()) {
					found = false;
					break;
				}
			}

			if (found) {
				selectedFields.append(it.mapId());
			}
		}
	}

	return selectedFields;
}

void CLI::commandCensus()
{
	ArgumentsCensus argsCensus;
	if (argsCensus.help() || argsCensus.path().isEmpty()) {
		argsCensus.showHelp();
	}

	FieldArchive *fieldArchive = openFieldArchive(argsCensus.inputFormat(), argsCensus.path());
	if (fieldArchive == nullptr) {
		return;
	}

	QList<int> selectedFields = selectFields(fieldArchive, argsCensus.includes(), argsCensus.excludes());

	Census census(fieldArchive, argsCensus.occurrences(), argsCensus.scripts());
	QJsonObject root = census.run(selectedFields);
	root["archive"] = argsCensus.path();

	QJsonDocument doc(root);
	QByteArray json = doc.toJson(argsCensus.pretty() ? QJsonDocument::Indented : QJsonDocument::Compact);

	if (argsCensus.output().isEmpty()) {
		std::cout.write(json.constData(), json.size());
		std::cout << std::endl;
	} else {
		QFile f(argsCensus.output());
		if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			qWarning() << qPrintable(QCoreApplication::translate("CLI", "Cannot write output file")) << qPrintable(f.errorString());
		} else {
			f.write(json);
			f.write("\n");
			f.close();
			qInfo() << qPrintable(QCoreApplication::translate("CLI", "Census written to")) << qPrintable(argsCensus.output())
			        << qPrintable(QCoreApplication::translate("CLI", "(%n field(s))", nullptr, int(selectedFields.size())));
		}
	}

	delete fieldArchive;
}

static bool applyOpsToField(FieldArchive *fieldArchive, Field *field, const QJsonArray &ops, QString &error, QStringList *describe)
{
	Section1File *section1 = field->scriptsAndTexts();
	if (section1 == nullptr || !section1->isOpen()) {
		error = "cannot read scripts";
		return false;
	}
	ScriptEdit edit(section1);
	if (!edit.apply(ops, error)) {
		return false;
	}
	int groupID, scriptID, opcodeID;
	if (!section1->compileScripts(groupID, scriptID, opcodeID, error)) {
		error = QString("compile failed at entity %1 script %2 opcode %3: %4").arg(groupID).arg(scriptID).arg(opcodeID).arg(error);
		return false;
	}
	if (describe != nullptr) {
		QList<QPair<int, int>> touched = edit.touched().values();
		std::sort(touched.begin(), touched.end());
		for (const QPair<int, int> &t : touched) {
			describe->append(edit.describe(t.first, t.second));
		}
	}
	section1->setModified(true);
	field->setModified(true);
	Q_UNUSED(fieldArchive)
	return true;
}

void CLI::commandScriptEditBatch(const ArgumentsScriptEdit &args)
{
	QDir dir(args.batchDir());
	QStringList files = dir.entryList(QStringList("*.json"), QDir::Files, QDir::Name);
	if (files.isEmpty()) {
		std::cerr << "No ops files in " << qPrintable(args.batchDir()) << std::endl;
		exit(1);
	}

	FieldArchive *fieldArchive = openFieldArchive(args.inputFormat(), args.path());
	if (fieldArchive == nullptr) {
		exit(1);
	}

	QJsonArray patched, failed;
	for (const QString &file : files) {
		QString name = QFileInfo(file).completeBaseName();
		QFile f(dir.filePath(file));
		QJsonObject entry;
		entry["field"] = name;
		if (!f.open(QIODevice::ReadOnly)) {
			entry["error"] = "cannot open ops file";
			failed.append(entry);
			continue;
		}
		QJsonParseError parseError;
		QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
		if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
			entry["error"] = "ops file is not a JSON array";
			failed.append(entry);
			continue;
		}
		QList<int> matches = selectFields(fieldArchive, QStringList(name), QStringList());
		Field *field = matches.size() == 1 ? fieldArchive->field(matches.first()) : nullptr;
		if (field == nullptr) {
			entry["error"] = "field not found";
			failed.append(entry);
			continue;
		}
		QString error;
		if (!applyOpsToField(fieldArchive, field, doc.array(), error, nullptr)) {
			entry["error"] = error;
			failed.append(entry);
			std::cout << "FAIL  " << qPrintable(name) << ": " << qPrintable(error) << std::endl;
			continue;
		}
		entry["ops"] = doc.array().size();
		patched.append(entry);
		std::cout << "ok    " << qPrintable(name) << std::endl;
	}

	if (!args.dryRun() && !patched.isEmpty()) {
		FieldArchiveIO::ErrorCode err = fieldArchive->save(args.targetFile());
		if (err != FieldArchiveIO::Ok) {
			std::cerr << "Save failed (error code " << int(err) << ")" << std::endl;
			delete fieldArchive;
			exit(1);
		}
		std::cout << "Saved " << qPrintable(args.targetFile()) << " (" << patched.size() << " fields patched, "
		          << failed.size() << " failed)" << std::endl;
	}

	if (!args.reportFile().isEmpty()) {
		QJsonObject report;
		report["archive"] = args.path();
		report["patched"] = patched;
		report["failed"] = failed;
		QFile rf(args.reportFile());
		if (rf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			rf.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
		}
	}
	delete fieldArchive;
	if (!failed.isEmpty()) {
		exit(2);
	}
}

void CLI::commandScriptEdit()
{
	ArgumentsScriptEdit args;
	if (!args.batchDir().isEmpty()) {
		if (args.path().isEmpty()) {
			args.showHelp();
		}
		commandScriptEditBatch(args);
		return;
	}
	if (args.help() || args.path().isEmpty() || args.field().isEmpty() || args.opsFile().isEmpty()) {
		args.showHelp();
	}

	QFile opsFile(args.opsFile());
	if (!opsFile.open(QIODevice::ReadOnly)) {
		qWarning() << qPrintable(QCoreApplication::translate("CLI", "Cannot open ops file")) << qPrintable(opsFile.errorString());
		exit(1);
	}
	QJsonParseError parseError;
	QJsonDocument opsDoc = QJsonDocument::fromJson(opsFile.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !opsDoc.isArray()) {
		qWarning() << qPrintable(QCoreApplication::translate("CLI", "Ops file must be a JSON array:")) << qPrintable(parseError.errorString());
		exit(1);
	}

	FieldArchive *fieldArchive = openFieldArchive(args.inputFormat(), args.path());
	if (fieldArchive == nullptr) {
		exit(1);
	}

	QList<int> matches = selectFields(fieldArchive, QStringList(args.field()), QStringList());
	Field *field = matches.size() == 1 ? fieldArchive->field(matches.first()) : nullptr;
	if (field == nullptr) {
		qWarning() << qPrintable(QCoreApplication::translate("CLI", "Field not found:")) << qPrintable(args.field());
		delete fieldArchive;
		exit(1);
	}
	Section1File *section1 = field->scriptsAndTexts();
	if (section1 == nullptr || !section1->isOpen()) {
		qWarning() << qPrintable(QCoreApplication::translate("CLI", "Cannot read scripts of field")) << qPrintable(args.field());
		delete fieldArchive;
		exit(1);
	}

	ScriptEdit edit(section1);
	QString error;
	if (!edit.apply(opsDoc.array(), error)) {
		std::cerr << "Edit failed: " << qPrintable(error) << std::endl;
		delete fieldArchive;
		exit(1);
	}

	int groupID, scriptID, opcodeID;
	if (!section1->compileScripts(groupID, scriptID, opcodeID, error)) {
		std::cerr << "Script compilation failed at entity " << groupID << " script " << scriptID << " opcode " << opcodeID << ": " << qPrintable(error) << std::endl;
		delete fieldArchive;
		exit(1);
	}

	QList<QPair<int, int>> touched = edit.touched().values();
	std::sort(touched.begin(), touched.end());
	for (const QPair<int, int> &t : touched) {
		std::cout << qPrintable(edit.describe(t.first, t.second)) << std::endl;
	}

	if (args.dryRun()) {
		qInfo() << qPrintable(QCoreApplication::translate("CLI", "Dry run: nothing saved."));
		delete fieldArchive;
		return;
	}

	section1->setModified(true);
	field->setModified(true);

	FieldArchiveIO::ErrorCode err = fieldArchive->save(args.targetFile());
	if (err != FieldArchiveIO::Ok) {
		qWarning() << qPrintable(QCoreApplication::translate("CLI", "Save failed (error code %1)").arg(int(err)));
		delete fieldArchive;
		exit(1);
	}
	std::cout << "Saved " << qPrintable(args.targetFile()) << std::endl;

	delete fieldArchive;
}

FieldArchive *CLI::openFieldArchive(const QString &ext, const QString &path)
{
	bool isPS;
	FieldArchiveIO::Type type;

	if (ext == "iso" || ext == "bin" || ext == "img") {
		isPS = true;
		type = FieldArchiveIO::Iso;
	} else {
		if (ext == "dat") {
			isPS = true;
			type = FieldArchiveIO::File;
		} else if (ext == "lgp") {
			isPS = false;
			type = FieldArchiveIO::Lgp;
		} else {
			isPS = false;
			type = FieldArchiveIO::File;
		}
	}

	FieldArchive *fieldArchive;

	if (isPS) {
		fieldArchive = new FieldArchivePS(path, type);
	} else {
		fieldArchive = new FieldArchivePC(path, type);
	}
	fieldArchive->setObserver(&observer);

	FieldArchiveIO::ErrorCode error = fieldArchive->open();

	QString out;
	switch (error)
	{
	case FieldArchiveIO::Ok:
	case FieldArchiveIO::Aborted:
		break;
	case FieldArchiveIO::FieldNotFound:
		out = QCoreApplication::translate("CLI", "Nothing found!");
		break;
	case FieldArchiveIO::FieldExists:
		out = QCoreApplication::translate("CLI", "The file already exists");
		break;
	case FieldArchiveIO::ErrorOpening:
		out = QCoreApplication::translate("CLI", "The file is inaccessible");
		break;
	case FieldArchiveIO::ErrorOpeningTemp:
		out = QCoreApplication::translate("CLI", "Can not create temporary file");
		break;
	case FieldArchiveIO::ErrorRemoving:
		out = QCoreApplication::translate("CLI", "Unable to remove the file, check write permissions.");
		break;
	case FieldArchiveIO::ErrorRenaming:
		out = QCoreApplication::translate("CLI", "Failed to rename the file, check write permissions.");
		break;
	case FieldArchiveIO::ErrorCopying:
		out = QCoreApplication::translate("CLI", "Failed to copy the file, check write permissions.");
		break;
	case FieldArchiveIO::Invalid:
		out = QCoreApplication::translate("CLI", "Invalid file");
		break;
	case FieldArchiveIO::NotImplemented:
		out = QCoreApplication::translate("CLI", "This error should not appear, thank you for reporting it");
		break;
	}

	if (!out.isEmpty()) {
		qWarning() << qPrintable(QCoreApplication::translate("CLI", "Error")) << qPrintable(out);
		delete fieldArchive;
		return nullptr;
	}

	return fieldArchive;
}

void CLI::exec()
{
	Arguments args;
	if (args.help()) {
		args.showHelp();
	}

	switch (args.command()) {
	case Arguments::None:
		args.showHelp();
	case Arguments::Export:
		commandExport();
		break;
	case Arguments::Patch:
		commandPatch();
		break;
	case Arguments::Tools:
		commandTools();
		break;
	case Arguments::Census:
		commandCensus();
		break;
	case Arguments::ScriptEdit:
		commandScriptEdit();
		break;
	}
}
