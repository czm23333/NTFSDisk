// Example.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include "NTFSDisk.h"
#include "Buffer.h"
#include <iostream>
#include <locale>
#include <codecvt>

int main(int argc, char* argv[])
{
	HANDLE hVolume = CreateFileA("\\\\.\\D:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	BOOT_SECTOR bootSector;
	bool success = ReadVolumeBootSector(hVolume, &bootSector);
	DebugBreak();

	FILE_RECORD fileRecord;
	unsigned long long offset = ReadFileRecordCluster(hVolume, &bootSector, &fileRecord, bootSector.sectorInfo.start$MFT);
	DebugBreak();

	{
		NONRESIDENT_ATTRIBUTE attr;
		PATTRIBUTE_HEAD attrHead = &attr.attributeHead;
		offset += fileRecord.firstAttributeOffset;
		while (ReadAttributeHead(hVolume, &bootSector, attrHead, offset) && attrHead->type != 0x80)
			offset += attrHead->length;
		DebugBreak();

		success = ReadAttribute(hVolume, &bootSector, &attr, offset);
		DebugBreak();

		offset += attr.runArrayOffset;
	}
	RUNLIST runList;
	offset = ReadRunList(hVolume, &bootSector, &runList, offset);
	DebugBreak();

	wchar_t name[1024];
	using convert_type = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type, wchar_t> converter;
	unsigned long long recordCnt = runList.size / bootSector.sectorInfo.fileRecordSize();
	for (unsigned i = 0; i < recordCnt; ++i) {
		offset = i * bootSector.sectorInfo.fileRecordSize();
		ReadRunListOffset(hVolume, &runList, &bootSector, &fileRecord, sizeof(fileRecord), offset);
		if (!fileRecord.legal()) continue;
		RESIDENT_ATTRIBUTE attr;
		ATTRIBUTE_FILENAME filename;
		filename.fileNameLength = 0;
		offset += fileRecord.firstAttributeOffset;
		bool flag = false;
		while (ReadRunListOffset(hVolume, &runList, &bootSector, &attr.attributeHead, sizeof(attr.attributeHead), offset) && attr.attributeHead.type != 0xFFFFFFFFu) {
			if (attr.attributeHead.type == 0x30u) {
				flag = true;
				ReadRunListOffset(hVolume, &runList, &bootSector, &attr, sizeof(attr), offset);
				unsigned long long tmp = offset;
				tmp += attr.dataOffset;
				ATTRIBUTE_FILENAME tmp2;
				ReadRunListOffset(hVolume, &runList, &bootSector, &tmp2, sizeof(tmp2), tmp);
				if (tmp2.fileNameLength >= filename.fileNameLength) {
					filename = tmp2;
					tmp += sizeof(ATTRIBUTE_FILENAME);
					ReadRunListOffset(hVolume, &runList, &bootSector, name, sizeof(wchar_t) * filename.fileNameLength, tmp);
				}
			}
			offset += attr.attributeHead.length;
		}
		if (!flag) {
			std::wprintf(L"<file without name>\n");
			continue; 
		}
		name[filename.fileNameLength] = L'\0';
		std::wprintf(L"%s\n", name);
	}
	CloseHandle(hVolume);
	return 0;
}