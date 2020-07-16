#pragma once
#include "Buffer.h"
#include <Windows.h>

#define NTFS_NORMAL_BYTES 512U

// NTFS struct

typedef union _BOOT_SECTOR {
	#pragma pack(1)
	struct BOOT_SECTOR_INFO {
		unsigned char jmpInstruction[3];
		unsigned char fileSystemId[8];
		unsigned short bytesPerSector;
		unsigned char sectorsPerCluster;
		unsigned short reservedSectors;
		// Always 00 00 00
		unsigned char constValue1[3];

		unsigned char unused1[2];
		unsigned char mediaDescriptor;
		unsigned char unused2[2];
		unsigned short sectorsPerTrack;
		unsigned short heads;
		unsigned int hiddenSectors;
		unsigned char unused3[4];
		// Always 80 00 80 00
		unsigned char constValue2[4];

		unsigned long long totalSectors;
		unsigned long long start$MFT;
		unsigned long long start$MFTMirr;
		char fileRecordSizeIndicator;
		unsigned char unused4[3];
		char indxBufferSizeIndicator;
		unsigned char unused5[3];
		union {
			unsigned char serialNumber32[4];
			unsigned char serialNumber64[8];
		};
		unsigned int checkSum;
		unsigned char otherData[426];
		// Always 55 AA
		unsigned char signature[2];
	} sectorInfo;
	typedef struct BOOT_SECTOR_INFO BOOT_SECTOR_INFO;
	unsigned char data[sizeof(BOOT_SECTOR_INFO)];
}BOOT_SECTOR, *PBOOT_SECTOR;

#pragma pack(1)
typedef struct _FILE_RECORD {
	// Always 46 49 4C 45
	unsigned char signature[4];

	unsigned short offsetUpdateSequence;
	// In words
	unsigned short sizeUpdateSequence;
	unsigned long long logfileSequenceNumber;
	unsigned short useDeletionCount;
	unsigned short hardLinkCount;
	unsigned short firstAttributeOffset;
	unsigned short flags;
	unsigned int logicalSize;
	unsigned int physicalSize;
	unsigned long long baseRecord; 
	unsigned int nextAttributeId;
	unsigned int recordId;
	unsigned char numberUpdateSequence[2];  
}FILE_RECORD, *PFILE_RECORD;

#pragma pack(1)
typedef struct _ATTRIBUTE_HEAD {
	unsigned int type;
	unsigned int length;
	unsigned char nonResident;
	unsigned char nameLength;
	unsigned short nameOffset;
	unsigned short flags;
	unsigned short attributeId;
} ATTRIBUTE_HEAD, *PATTRIBUTE_HEAD;

#pragma pack(1)
typedef struct _RESIDENT_ATTRIBUTE {
	_ATTRIBUTE_HEAD attributeHead; 
	unsigned int dataLength;
	unsigned short dataOffset;
	unsigned char flags;
	// Always 00
	unsigned char constValue1;
} RESIDENT_ATTRIBUTE, *PRESIDENT_ATTRIBUTE;

#pragma pack(1)
typedef struct _NONRESIDENT_ATTRIBUTE {
	_ATTRIBUTE_HEAD attributeHead;
	unsigned long long startVcn;
	unsigned long long lastVcn;
	unsigned short runArrayOffset;
	unsigned short compressionUnit;
	// Always 00 00 00 00
	unsigned char constValue1[4]; 

	unsigned long long size;
	unsigned long long allocatedSize;
	unsigned long long unkownMeaning;
} NONRESIDENT_ATTRIBUTE, *PNONRESIDENT_ATTRIBUTE;


// Info storing struct

typedef struct _RUNLIST_ENTRY_INFO {
	unsigned long long clusterTaken;
	unsigned long long startVcn;
}RUNLIST_ENTRY_INFO, *PRUNLIST_ENTRY_INFO;


// Intefaces

bool ReadVolumeBootSector(_In_ HANDLE hVolume, _Out_ PBOOT_SECTOR pBootSector);
bool ReadVolumeOffset(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ void* buffer, _In_ size_t size, _In_ unsigned long long offset);
bool ReadVolumeOffset(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ BUFFER buffer, _In_ unsigned long long offset);
bool ReadVolumeOffset(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ CUcharBuffer& buffer, _In_ unsigned long long offset);
bool ReadVolumeSector(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ void* buffer, _In_ size_t size, _In_ unsigned long long sector);
bool ReadVolumeSector(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ BUFFER buffer, _In_ unsigned long long sector);
bool ReadVolumeSector(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ CUcharBuffer& buffer, _In_ unsigned long long sector);
bool ReadVolumeCluster(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ void* buffer, _In_ size_t size, _In_ unsigned long long cluster);
bool ReadVolumeCluster(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ BUFFER buffer, _In_ unsigned long long cluster);
bool ReadVolumeCluster(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ CUcharBuffer& buffer, _In_ unsigned long long cluster);

// Return the offset of the record
unsigned long long ReadFileRecord(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ PFILE_RECORD pFileRecord, _In_ unsigned long long sector);
unsigned long long ReadFileRecordCluster(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ PFILE_RECORD pFileRecord, _In_ unsigned long long cluster);

bool ReadAttributeHead(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ PATTRIBUTE_HEAD pAttributeHead, _In_ unsigned long long offset);
bool ReadAttribute(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ PRESIDENT_ATTRIBUTE pAttribute, _In_ unsigned long long offset);
bool ReadAttribute(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ PNONRESIDENT_ATTRIBUTE pAttribute, _In_ unsigned long long offset);
// Return the offset of the next entry
unsigned long long ReadRunListEntry(_In_ HANDLE hVolume, _In_ PBOOT_SECTOR pBootSector, _Out_ PRUNLIST_ENTRY_INFO pEntryInfo, _In_ unsigned long long offset);