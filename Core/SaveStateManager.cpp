#include "stdafx.h"
#include "../Utilities/FolderUtilities.h"
#include "../Utilities/ZipWriter.h"
#include "../Utilities/ZipReader.h"
#include "SaveStateManager.h"
#include "MessageManager.h"
#include "Console.h"
#include "EmulationSettings.h"
#include "VideoDecoder.h"
#include "Debugger.h"
#include "MovieManager.h"

const uint32_t SaveStateManager::FileFormatVersion;
atomic<uint32_t> SaveStateManager::_lastIndex(1);

string SaveStateManager::GetStateFilepath(int stateIndex)
{
	string folder = FolderUtilities::GetSaveStateFolder();
	string filename = FolderUtilities::GetFilename(Console::GetMapperInfo().RomName, false) + "_" + std::to_string(stateIndex) + ".mst";
	return FolderUtilities::CombinePath(folder, filename);
}

uint64_t SaveStateManager::GetStateInfo(int stateIndex)
{
	string filepath = SaveStateManager::GetStateFilepath(stateIndex);
	ifstream file(filepath, ios::in | ios::binary);

	if(file) {
		file.close();
		return FolderUtilities::GetFileModificationTime(filepath);
	}
	return 0;
}

void SaveStateManager::MoveToNextSlot()
{
	_lastIndex = (_lastIndex % MaxIndex) + 1;
	MessageManager::DisplayMessage("SaveStates", "SaveStateSlotSelected", std::to_string(_lastIndex));
}

void SaveStateManager::MoveToPreviousSlot()
{
	_lastIndex = (_lastIndex == 1 ? SaveStateManager::MaxIndex : (_lastIndex - 1));
	MessageManager::DisplayMessage("SaveStates", "SaveStateSlotSelected", std::to_string(_lastIndex));
}

void SaveStateManager::SaveState()
{
	SaveState(_lastIndex);
}

bool SaveStateManager::LoadState()
{
	return LoadState(_lastIndex);
}

void SaveStateManager::SaveState(ostream &stream)
{
	uint32_t emuVersion = EmulationSettings::GetMesenVersion();
	stream.write("MST", 3);
	stream.write((char*)&emuVersion, sizeof(emuVersion));
	stream.write((char*)&SaveStateManager::FileFormatVersion, sizeof(uint32_t));

	MapperInfo mapperInfo = Console::GetMapperInfo();
	stream.write((char*)&mapperInfo.MapperId, sizeof(uint16_t));
	stream.write((char*)&mapperInfo.SubMapperId, sizeof(uint8_t));
	
	string sha1Hash = mapperInfo.Hash.Sha1Hash;
	stream.write(sha1Hash.c_str(), sha1Hash.size());

	string romName = mapperInfo.RomName;
	uint32_t nameLength = (uint32_t)romName.size();
	stream.write((char*)&nameLength, sizeof(uint32_t));
	stream.write(romName.c_str(), romName.size());

	Console::SaveState(stream);
}

bool SaveStateManager::SaveState(string filepath)
{
	ofstream file(filepath, ios::out | ios::binary);

	if(file) {
		Console::Pause();
		SaveState(file);
		file.close();

		shared_ptr<Debugger> debugger = Console::GetInstance()->GetDebugger(false);
		if(debugger) {
			debugger->ProcessEvent(EventType::StateSaved);
		}

		Console::Resume();
		return true;
	}
	return false;
}

void SaveStateManager::SaveState(int stateIndex, bool displayMessage)
{
	string filepath = SaveStateManager::GetStateFilepath(stateIndex);
	if(SaveState(filepath)) {
		if(displayMessage) {
			MessageManager::DisplayMessage("SaveStates", "SaveStateSaved", std::to_string(stateIndex));
		}
	}
}

bool SaveStateManager::LoadState(istream &stream, bool hashCheckRequired)
{
	char header[3];
	stream.read(header, 3);
	if(memcmp(header, "MST", 3) == 0) {
		uint32_t emuVersion, fileFormatVersion;

		stream.read((char*)&emuVersion, sizeof(emuVersion));
		if(emuVersion > EmulationSettings::GetMesenVersion()) {
			MessageManager::DisplayMessage("SaveStates", "SaveStateNewerVersion");
			return false;
		}

		stream.read((char*)&fileFormatVersion, sizeof(fileFormatVersion));
		if(fileFormatVersion < 5) {
			MessageManager::DisplayMessage("SaveStates", "SaveStateIncompatibleVersion");
			return false;
		} else if(fileFormatVersion == 5) {
			//No SHA1 field in version 5
			if(hashCheckRequired) {
				//Can't manually load < v5 save states, since we can't know what game the save state is for
				MessageManager::DisplayMessage("SaveStates", "SaveStateIncompatibleVersion");
				return false;
			}
		} else {
			int32_t mapperId = -1;
			int32_t subMapperId = -1;
			if(fileFormatVersion >= 8) {
				uint16_t id;
				uint8_t sid;
				stream.read((char*)&id, sizeof(uint16_t));
				stream.read((char*)&sid, sizeof(uint8_t));
				mapperId = id;
				subMapperId = sid;
			}

			char hash[41] = {};
			stream.read(hash, 40);

			uint32_t nameLength = 0;
			stream.read((char*)&nameLength, sizeof(uint32_t));
			
			vector<char> nameBuffer(nameLength);
			stream.read(nameBuffer.data(), nameBuffer.size());
			string romName(nameBuffer.data(), nameLength);
			
			MapperInfo mapperInfo = Console::GetMapperInfo();
			bool gameLoaded = !mapperInfo.Hash.Sha1Hash.empty();
			if(mapperInfo.Hash.Sha1Hash != string(hash)) {
				//CRC doesn't match
				if(!EmulationSettings::CheckFlag(EmulationFlags::AllowMismatchingSaveState) || !gameLoaded ||
					mapperInfo.MapperId != mapperId || mapperInfo.SubMapperId != subMapperId) 
				{
					//If mismatching states aren't allowed, or a game isn't loaded, or the mapper types don't match, try to find and load the matching ROM
					HashInfo info;
					info.Sha1Hash = hash;
					if(!Console::LoadROM(romName, info)) {
						MessageManager::DisplayMessage("SaveStates", "SaveStateMissingRom", romName);
						return false;
					}
				}
			}
		}

		//Stop any movie that might have been playing/recording if a state is loaded
		//(Note: Loading a state is disabled in the UI while a movie is playing/recording)
		MovieManager::Stop();

		Console::LoadState(stream, fileFormatVersion);

		return true;
	}
	MessageManager::DisplayMessage("SaveStates", "SaveStateInvalidFile");
	return false;
}

bool SaveStateManager::LoadState(string filepath, bool hashCheckRequired)
{
	ifstream file(filepath, ios::in | ios::binary);
	bool result = false;

	if(file.good()) {
		Console::Pause();
		if(LoadState(file, hashCheckRequired)) {
			result = true;
		}
		file.close();
		shared_ptr<Debugger> debugger = Console::GetInstance()->GetDebugger(false);
		if(debugger) {
			debugger->ProcessEvent(EventType::StateLoaded);
		}
		Console::Resume();
	} else {
		MessageManager::DisplayMessage("SaveStates", "SaveStateEmpty");
	}

	return result;
}

bool SaveStateManager::LoadState(int stateIndex)
{
	string filepath = SaveStateManager::GetStateFilepath(stateIndex);
	if(LoadState(filepath, false)) {
		MessageManager::DisplayMessage("SaveStates", "SaveStateLoaded", std::to_string(stateIndex));
		return true;
	}
	return false;
}

void SaveStateManager::SaveRecentGame(string romName, string romPath, string patchPath)
{
	if(!EmulationSettings::CheckFlag(EmulationFlags::ConsoleMode) && !EmulationSettings::CheckFlag(EmulationFlags::DisableGameSelectionScreen) && Console::GetMapperInfo().Format != RomFormat::Nsf) {
		string filename = FolderUtilities::GetFilename(Console::GetMapperInfo().RomName, false) + ".rgd";
		ZipWriter writer;
		writer.Initialize(FolderUtilities::CombinePath(FolderUtilities::GetRecentGamesFolder(), filename));

		std::stringstream pngStream;
		VideoDecoder::GetInstance()->TakeScreenshot(pngStream);
		writer.AddFile(pngStream, "Screenshot.png");

		std::stringstream stateStream;
		SaveStateManager::SaveState(stateStream);
		writer.AddFile(stateStream, "Savestate.mst");

		std::stringstream romInfoStream;
		romInfoStream << romName << std::endl;
		romInfoStream << romPath << std::endl;
		romInfoStream << patchPath << std::endl;
		writer.AddFile(romInfoStream, "RomInfo.txt");
		writer.Save();
	}
}

void SaveStateManager::LoadRecentGame(string filename, bool resetGame)
{
	ZipReader reader;
	reader.LoadArchive(filename);

	stringstream romInfoStream, stateStream;
	reader.GetStream("RomInfo.txt", romInfoStream);
	reader.GetStream("Savestate.mst", stateStream);

	string romName, romPath, patchPath;
	std::getline(romInfoStream, romName);
	std::getline(romInfoStream, romPath);
	std::getline(romInfoStream, patchPath);

	Console::Pause();
	try {
		if(Console::LoadROM(romPath, patchPath)) {
			if(!resetGame) {
				SaveStateManager::LoadState(stateStream, false);
			}
		}
	} catch(std::exception ex) { 
		Console::GetInstance()->Stop();
	}
	Console::Resume();
}