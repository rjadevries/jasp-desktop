//
// Copyright (C) 2013-2018 University of Amsterdam
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public
// License along with this program.  If not, see
// <http://www.gnu.org/licenses/>.
//

#include "filemenu.h"

#include <QFileInfo>
#include "utilities/settings.h"
#include "gui/messageforwarder.h"

FileMenu::FileMenu(QObject *parent) : QObject(parent)
{	
	_RecentFiles			= new RecentFiles(parent);
	_CurrentFile			= new CurrentFile(parent);
	_Computer				= new Computer(parent);
	_OSF					= new OSF(parent);
	_DataLibrary			= new DataLibrary(parent);
	_actionButtons			= new ActionButtons(this);
	_resourceButtons		= new ResourceButtons(this);
	_resourceButtonsVisible	= new ResourceButtonsVisible(this, _resourceButtons);

	connect(_RecentFiles,		&FileMenuObject::dataSetIORequest,		this, &FileMenu::dataSetIORequestHandler);
	connect(_CurrentFile,		&FileMenuObject::dataSetIORequest,		this, &FileMenu::dataSetIORequestHandler);
	connect(_Computer,			&FileMenuObject::dataSetIORequest,		this, &FileMenu::dataSetIORequestHandler);
	connect(_OSF,				&FileMenuObject::dataSetIORequest,		this, &FileMenu::dataSetIORequestHandler);
	connect(_DataLibrary,		&FileMenuObject::dataSetIORequest,		this, &FileMenu::dataSetIORequestHandler);
	connect(_actionButtons,		&ActionButtons::buttonClickedSignal,	this, &FileMenu::fileOperationClicked);
	connect(&_watcher,			&QFileSystemWatcher::fileChanged,		this, &FileMenu::dataFileModifiedHandler);
	connect(_resourceButtons,	&ResourceButtons::clicked,				this, &FileMenu::resourceButtonClicked);

	_actionButtons->setEnabled(ActionButtons::Open,				true);
	_actionButtons->setEnabled(ActionButtons::Save,				false);
	_actionButtons->setEnabled(ActionButtons::SaveAs,			false);
	_actionButtons->setEnabled(ActionButtons::ExportResults,	false);
	_actionButtons->setEnabled(ActionButtons::ExportData,		false);
	_actionButtons->setEnabled(ActionButtons::SyncData,			false);
	_actionButtons->setEnabled(ActionButtons::Close,			false);

	setFileoperation(ActionButtons::Open);
}

void FileMenu::setFileoperation(ActionButtons::FileOperation fo)
{
	_fileoperation = fo;
	emit fileoperationChanged();

	switch (fo)
	{
	case ActionButtons::FileOperation::Open:
		_resourceButtons->setOnlyTheseButtonsVisible({ResourceButtons::RecentFiles,	ResourceButtons::Computer,	ResourceButtons::DataLibrary, ResourceButtons::OSF});
		break;

	case ActionButtons::FileOperation::SyncData:
		_resourceButtons->setOnlyTheseButtonsVisible({ResourceButtons::CurrentFile, ResourceButtons::Computer, ResourceButtons::OSF});
		break;

	case ActionButtons::FileOperation::SaveAs:
	case ActionButtons::FileOperation::ExportData:
	case ActionButtons::FileOperation::ExportResults:
		_resourceButtons->setOnlyTheseButtonsVisible({ResourceButtons::Computer, ResourceButtons::OSF});
		break;

	case ActionButtons::Preferences:
		_resourceButtons->setOnlyTheseButtonsVisible({ResourceButtons::PrefsData, ResourceButtons::PrefsResults, ResourceButtons::PrefsAdvanced});
		break;

	default:
		_resourceButtons->setOnlyTheseButtonsVisible();
		break;

	}
}

void FileMenu::setSaveMode(FileEvent::FileMode mode)
{
	_mode = mode;

	_Computer->setMode(_mode);
	
	_OSF->setMode(_mode);
	_OSF->setCurrentFileName(getDefaultOutFileName());
}

void FileMenu::setOnlineDataManager(OnlineDataManager *odm)
{
	_odm = odm;
	_OSF->setOnlineDataManager(odm);
}

FileEvent *FileMenu::open(const QString &path)
{
	FileEvent *event = new FileEvent(this, FileEvent::FileOpen);
	event->setPath(path);
	dataSetIORequestHandler(event);

	return event;
}

FileEvent *FileMenu::save()
{
	FileEvent *event;

	if (_currentFileType != Utils::FileType::jasp || _currentFileReadOnly)
	{
		event = _Computer->browseSave();
		if (event->isCompleted())
			return event;
	}
	else
	{
		event = new FileEvent(this, FileEvent::FileSave);
		if (!event->setPath(_currentFilePath))
		{
			MessageForwarder::showWarning("File Types", event->getLastError());
			event->setComplete(false, "Failed to open file from OSF");
			return event;
		}
	}

	dataSetIORequestHandler(event);

	return event;
}

void FileMenu::sync()
{
	QString path = _CurrentFile->getCurrentDataFilePath();

	if (path.isEmpty())
	{
		if(!MessageForwarder::showYesNo("No associated data file",
					"JASP has no associated data file (csv, sav or ods file) to be synchronized with. "
					"Do you want to search for such a data file on your computer?\nNB: You can also set this data file via menu File/Sync Data."))
			return;

		path =  MessageForwarder::openFileBrowse("Find Data File", "", "Data File (*.csv *.txt *.sav *.ods)");
	}

	dataSetOpenCurrentRequestHandler(path);
	
}

FileEvent *FileMenu::close()
{
	FileEvent *event = new FileEvent(this, FileEvent::FileClose);
	dataSetIORequestHandler(event);

	setFileoperation(ActionButtons::FileOperation::Open);
	return event;
}

void FileMenu::setCurrentDataFile(const QString &path)
{
	QString currentPath = _CurrentFile->getCurrentDataFilePath();
	if (!currentPath.isEmpty())
		_watcher.removePath(currentPath);

	bool setCurrentPath = true;
	bool enableCurrentTab = false;
	if (!path.isEmpty())
	{
		if (checkSyncFileExists(path))
		{
			enableCurrentTab = true;
			int sync = Settings::value(Settings::DATA_AUTO_SYNCHRONIZATION).toInt();
			if (sync > 0)
				_watcher.addPath(path);
		}
		else
			setCurrentPath = false;
	}

	if (setCurrentPath)
		_CurrentFile->setCurrentDataFilePath(path);
}

void FileMenu::setDataFileWatcher(bool watch)
{
	QString path = _CurrentFile->getCurrentFilePath();
	if (!path.isEmpty())
	{
		if (watch && !_CurrentFile->isOnlineFile(path))
			_watcher.addPath(path);
		else
			_watcher.removePath(path);
	}
}


QString FileMenu::getDefaultOutFileName()
{
	QString path				= getCurrentFilePath(),
			DefaultOutFileName	= "";

	if (path != "")
	{
		QString name	= QFileInfo(path).baseName(),
				ext		= QFileInfo(path).suffix();
		switch (_mode)
		{
		case FileEvent::FileSave:			ext = "jasp";	break;
		case FileEvent::FileExportResults:	ext = "html";	break;
		case FileEvent::FileExportData:
		case FileEvent::FileGenerateData:	ext = "csv";	break;
		default:											break;
		}
		DefaultOutFileName = name + "." + ext;
	}

	return DefaultOutFileName;
	
}


void FileMenu::dataSetIOCompleted(FileEvent *event)
{
	if (event->operation() == FileEvent::FileSave || event->operation() == FileEvent::FileOpen)
	{
		if (event->successful())
		{
			//  don't add examples to the recent list
			if (!event->isReadOnly())
			{
				_RecentFiles->pushRecentFilePath(event->path());
				_Computer->addRecentFolder(event->path());
			}

			if (event->operation() == FileEvent::FileOpen && !event->isReadOnly())
				setCurrentDataFile(event->dataFilePath());

			// all this stuff is a hack
			QFileInfo info(event->path());
			_Computer->setFileName(info.baseName());

			_currentFilePath		= event->path();
			_currentFileType		= event->type();
			_currentFileReadOnly	= event->isReadOnly();
			_CurrentFile->setCurrentFileInfo(event->path(), event->type(), event->isReadOnly());
			_OSF->setProcessing(false);
		}
	}
	else if (event->operation() == FileEvent::FileSyncData)
	{
		if (event->successful())		setCurrentDataFile(event->dataFilePath());
		else
			std::cout << "Sync failed: " << event->getLastError().toStdString() << std::endl;
	}
	else if (event->operation() == FileEvent::FileClose)
	{
		_Computer->clearFileName();
		_currentFilePath = "";
		_currentFileType = Utils::FileType::unknown;
		_currentFileReadOnly = false;
		_CurrentFile->setCurrentFileInfo("", Utils::FileType::unknown, false);
		clearSyncData();
	}
		
	if (event->successful())
	{
		if (event->operation() == FileEvent::FileOpen)
		{
			
			_actionButtons->setEnabled(ActionButtons::Save, event->type() == Utils::FileType::jasp);
			_actionButtons->setEnabled(ActionButtons::SaveAs, true);
			_actionButtons->setEnabled(ActionButtons::ExportResults, true);
			_actionButtons->setEnabled(ActionButtons::ExportData, true);
			_actionButtons->setEnabled(ActionButtons::SyncData, true);
			_actionButtons->setEnabled(ActionButtons::Close, true);
		}
		else if (event->operation() == FileEvent::FileSave)
		{
			_actionButtons->setEnabled(ActionButtons::Save, true);
		}
		else if (event->operation() == FileEvent::FileClose)
		{
			_actionButtons->setEnabled(ActionButtons::Save, false);
			_actionButtons->setEnabled(ActionButtons::SaveAs, false);
			_actionButtons->setEnabled(ActionButtons::ExportResults, false);
			_actionButtons->setEnabled(ActionButtons::ExportData, false);
			_actionButtons->setEnabled(ActionButtons::SyncData, false);
			_actionButtons->setEnabled(ActionButtons::Close, false);
		}
	}
}

void FileMenu::dataFileModifiedHandler(QString path)
{

	int autoSync = Settings::value(Settings::DATA_AUTO_SYNCHRONIZATION).toInt();
	if (autoSync > 0)
		dataSetOpenCurrentRequestHandler(path);
	
}

void FileMenu::dataSetIORequestHandler(FileEvent *event)
{
	connect(event, &FileEvent::completed,		this, &FileMenu::dataSetIOCompleted			);
	connect(event, &FileEvent::dataFileChanged, this, &FileMenu::dataFileModifiedHandler	);

	emit dataSetIORequest(event);
	
}

void FileMenu::dataSetOpenExampleRequestHandler(QString path)
{
	FileEvent *event = new FileEvent(this);
	event->setPath(path);
	event->setReadOnly();

	dataSetIORequestHandler(event);
}

void FileMenu::analysisAdded(Analysis *analysis)
{
	_actionButtons->setEnabled(ActionButtons::SaveAs, true);
	_actionButtons->setEnabled(ActionButtons::ExportResults, true);
}

void FileMenu::setSyncFile(FileEvent *event)
{
	if (event->successful())
		setCurrentDataFile(event->path());
}

void FileMenu::fileOperationClicked(const ActionButtons::FileOperation action)
{
	setFileoperation(action);
	
	switch (action)
	{
	case ActionButtons::FileOperation::Open:				setSaveMode(FileEvent::FileOpen);			break;
	case ActionButtons::FileOperation::SaveAs:				setSaveMode(FileEvent::FileSave);			break;
	case ActionButtons::FileOperation::ExportResults:		setSaveMode(FileEvent::FileExportResults);	break;
	case ActionButtons::FileOperation::ExportData:  		setSaveMode(FileEvent::FileExportData);		break;
	case ActionButtons::FileOperation::SyncData:			setSaveMode(FileEvent::FileSyncData);		break;
	case ActionButtons::FileOperation::Save:
		if (getCurrentFileType() == Utils::FileType::jasp && ! isCurrentFileReadOnly())
			save();
		else
			setSaveMode(FileEvent::FileSave);			
		break;


	case ActionButtons::FileOperation::Close:
		close();
		setSaveMode(FileEvent::FileOpen);
		break;
	}
}

void FileMenu::resourceButtonClicked(const int buttonType)
{
	if (buttonType == ResourceButtons::OSF)
		_OSF->attemptToConnect();
}

void FileMenu::dataSetOpenCurrentRequestHandler(QString path)
{
	if (path.isEmpty())
		return;

	if (checkSyncFileExists(path))
	{
		FileEvent *event = new FileEvent(this, FileEvent::FileSyncData);
		event->setPath(path);

		dataSetIORequestHandler(event);
	}
}

bool FileMenu::checkSyncFileExists(const QString &path)
{
	bool exists = path.startsWith("http") ? true : (QFileInfo::exists(path) && Utils::getFileSize(path.toStdString()) > 0);
    if (!exists)
	{
        int attempts = 1;
        while (!exists && attempts < 20)
        {
            Utils::sleep(100);
            attempts++;
            exists = QFileInfo::exists(path) && Utils::getFileSize(path.toStdString()) > 0;
        }
    }
    if (!exists)
    {
#ifdef JASP_DEBUG
        std::cout << "Sync file does not exist: " << path.toStdString() << std::endl;
#endif
		clearSyncData();
	}

	return exists;
}


void FileMenu::clearSyncData()
{
	setDataFileWatcher(false); // must be done before setting the current to empty.
	_CurrentFile->setCurrentDataFilePath(QString());
}

bool FileMenu::clearOSFFromRecentList(QString path)
{
	return OnlineDataManager::determineProvider(path) != OnlineDataManager::OSF;
}

void FileMenu::setVisible(bool visible)
{
	if (_visible == visible)
		return;

	_visible = visible;
	emit visibleChanged(_visible);
}

void FileMenu::showFileMenu()
{
	setVisible(true);
	_actionButtons->buttonClicked(ActionButtons::Open);
}
