// Example.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include "NTFSDisk.h"
#include "Buffer.h"

int main(int argc, char* argv[])
{
	HANDLE hVolume = CreateFileA("\\\\.\\D:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	BOOT_SECTOR bootSector;
	bool success = ReadVolumeBootSector(hVolume, &bootSector);
	DebugBreak();

	FILE_RECORD fileRecord;
	unsigned long long offset = ReadFileRecordCluster(hVolume, &bootSector, &fileRecord, bootSector.sectorInfo.start$MFT);
	DebugBreak();

	NONRESIDENT_ATTRIBUTE attr;
	PATTRIBUTE_HEAD attrHead = &attr.attributeHead;
	offset += fileRecord.firstAttributeOffset;
	while (ReadAttributeHead(hVolume, &bootSector, attrHead, offset) && attrHead->type != 0x80)
		offset += attrHead->length;
	DebugBreak();

	success = ReadAttribute(hVolume, &bootSector, &attr, offset);
	DebugBreak();

	offset += attr.runArrayOffset;
	RUNLIST_ENTRY_INFO info;
	success = ReadRunListEntry(hVolume, &bootSector, &info, offset);
	CloseHandle(hVolume);
	DebugBreak();
	return 0;
}