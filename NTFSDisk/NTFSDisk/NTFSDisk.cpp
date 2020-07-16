#include "stdafx.h"
#include "NTFSDisk.h"
#include "Buffer.h"
#include <utility>

static union { unsigned char c[4]; unsigned int l; }endian_test = { 1 };
static bool needToConvert = !((bool)endian_test.c[0]);
#define ushort_ec(x) (x = \
((((x) & (unsigned short)0x00ffU) << 8) | \
(((x) & (unsigned short)0xff00U) >> 8)))
#define uint_ec(x) (x = \
((((x) & 0xff000000U) >> 24) | \
(((x) & 0x00ff0000U) >> 8) | \
(((x) & 0x0000ff00U) << 8) | \
(((x) & 0x000000ffU) << 24)))
#define ull_ec(x) (x = \
((((x) & 0xff00000000000000ULL) >> 56) | \
(((x) & 0x00ff000000000000ULL) >> 40) | \
(((x) & 0x0000ff0000000000ULL) >> 24) | \
(((x) & 0x000000ff00000000ULL) >> 8) | \
(((x) & 0x00000000000000ffULL) << 56) | \
(((x) & 0x000000000000ff00ULL) << 40) | \
(((x) & 0x0000000000ff0000ULL) << 24) | \
(((x) & 0x00000000ff000000ULL) << 8)))
#define decompress_uchar(x) { x / 0x10U, x % 0x10U }

bool ReadVolumeOffset(HANDLE hVolume, PBOOT_SECTOR pBootSector, void* buffer, size_t size, unsigned long long offset) {
	const unsigned long long bps = pBootSector->sectorInfo.bytesPerSector;
	const unsigned long long dataOffset = offset % bps;
	if (dataOffset != 0) {
		offset -= dataOffset;
		size += dataOffset;
	}

	LARGE_INTEGER ptr;
	ptr.QuadPart = offset;
	SetFilePointer(hVolume, ptr.LowPart, &ptr.HighPart, FILE_BEGIN);
	if (ptr.HighPart == INVALID_SET_FILE_POINTER)
		return false;

	DWORD bytes;
	if (size % bps != 0) {
		const size_t newSize = (size / bps + 1) * bps;
		CUcharBuffer temp(newSize);
		bool success;
		if (success = ReadFile(hVolume, temp, newSize, &bytes, NULL) && bytes >= size) {
			memcpy_s(buffer, size - dataOffset, temp + dataOffset, size - dataOffset);
		}
		return success;
	}
	else if (dataOffset) {
		CUcharBuffer temp(size);
		bool success;
		if (success = ReadFile(hVolume, temp, size, &bytes, NULL) && bytes == size) {
			memcpy_s(buffer, size - dataOffset, temp + dataOffset, size - dataOffset);
		}
		return success;
	}
	else
		return ReadFile(hVolume, buffer, size, &bytes, NULL) && bytes == size;
}

bool ReadVolumeOffset(HANDLE hVolume, PBOOT_SECTOR pBootSector, BUFFER buffer, unsigned long long offset) {
	return ReadVolumeOffset(hVolume, pBootSector, buffer.buffer, buffer.size, offset);
}

bool ReadVolumeOffset(HANDLE hVolume, PBOOT_SECTOR pBootSector, CUcharBuffer& buffer, unsigned long long offset) {
	return buffer.allocated && ReadVolumeOffset(hVolume, pBootSector, buffer.buffer, buffer.size, offset);
}

bool ReadVolumeSector(HANDLE hVolume, PBOOT_SECTOR pBootSector, void* buffer, size_t size, unsigned long long sector) {
	return ReadVolumeOffset(hVolume, pBootSector, buffer, size, sector * pBootSector->sectorInfo.bytesPerSector);
}

bool ReadVolumeSector(HANDLE hVolume, PBOOT_SECTOR pBootSector, BUFFER buffer, unsigned long long sector) {
	return ReadVolumeSector(hVolume, pBootSector, buffer.buffer, buffer.size, sector);
}

bool ReadVolumeSector(HANDLE hVolume, PBOOT_SECTOR pBootSector, CUcharBuffer& buffer, unsigned long long sector) {
	if (!buffer.allocated)
		buffer = std::move(CUcharBuffer(pBootSector->sectorInfo.bytesPerSector));
	return ReadVolumeSector(hVolume, pBootSector, buffer.buffer, buffer.size, sector);
}

bool ReadVolumeCluster(HANDLE hVolume, PBOOT_SECTOR pBootSector, void* buffer, size_t size, unsigned long long cluster) {
	return ReadVolumeSector(hVolume, pBootSector, buffer, size, cluster * pBootSector->sectorInfo.sectorsPerCluster);
}

bool ReadVolumeCluster(HANDLE hVolume, PBOOT_SECTOR pBootSector, BUFFER buffer, unsigned long long cluster) {
	return ReadVolumeCluster(hVolume, pBootSector, buffer.buffer, buffer.size, cluster);
}

bool ReadVolumeCluster(HANDLE hVolume, PBOOT_SECTOR pBootSector, CUcharBuffer& buffer, unsigned long long cluster) {
	if (!buffer.allocated)
		buffer = std::move(CUcharBuffer((size_t)pBootSector->sectorInfo.sectorsPerCluster * pBootSector->sectorInfo.bytesPerSector));
	return ReadVolumeCluster(hVolume, pBootSector, buffer.buffer, buffer.size, cluster);
}

bool ReadVolumeBootSector(HANDLE hVolume, PBOOT_SECTOR pBootSector) {
	SetFilePointer(hVolume, NULL, NULL, FILE_BEGIN);

	DWORD bytes;
	if (!ReadFile(hVolume, pBootSector->data, NTFS_NORMAL_BYTES, &bytes, NULL) || bytes < NTFS_NORMAL_BYTES)
		return false;

	if (needToConvert) {
		ushort_ec(pBootSector->sectorInfo.bytesPerSector);
		ushort_ec(pBootSector->sectorInfo.reservedSectors);
		ushort_ec(pBootSector->sectorInfo.sectorsPerTrack);
		ushort_ec(pBootSector->sectorInfo.heads);
		uint_ec(pBootSector->sectorInfo.hiddenSectors);
		ull_ec(pBootSector->sectorInfo.totalSectors);
		ull_ec(pBootSector->sectorInfo.start$MFT);
		ull_ec(pBootSector->sectorInfo.start$MFTMirr);
		uint_ec(pBootSector->sectorInfo.checkSum);
	}

	return true;
}

unsigned long long ReadFileRecord(HANDLE hVolume, PBOOT_SECTOR pBootSector, PFILE_RECORD pFileRecord, unsigned long long sector) {
	if (!ReadVolumeSector(hVolume, pBootSector, pFileRecord, sizeof(FILE_RECORD), sector))
		return 0ULL;

	if (needToConvert) {
		ushort_ec(pFileRecord->offsetUpdateSequence);
		ushort_ec(pFileRecord->sizeUpdateSequence);
		ull_ec(pFileRecord->logfileSequenceNumber);
		ushort_ec(pFileRecord->useDeletionCount);
		ushort_ec(pFileRecord->hardLinkCount);
		ushort_ec(pFileRecord->firstAttributeOffset);
		ushort_ec(pFileRecord->flags);
		uint_ec(pFileRecord->logicalSize);
		uint_ec(pFileRecord->physicalSize);
		ull_ec(pFileRecord->baseRecord);
		uint_ec(pFileRecord->nextAttributeId);
		uint_ec(pFileRecord->recordId);
	}

	return sector * pBootSector->sectorInfo.bytesPerSector;
}

unsigned long long ReadFileRecordCluster(HANDLE hVolume, PBOOT_SECTOR pBootSector, PFILE_RECORD pFileRecord, unsigned long long cluster) {
	return ReadFileRecord(hVolume, pBootSector, pFileRecord, cluster * pBootSector->sectorInfo.sectorsPerCluster);
}

bool ReadAttributeHead(HANDLE hVolume, PBOOT_SECTOR pBootSector, PATTRIBUTE_HEAD pAttributeHead, unsigned long long offset) {
	if (!ReadVolumeOffset(hVolume, pBootSector, pAttributeHead, sizeof(ATTRIBUTE_HEAD), offset))
		return false;

	if (needToConvert) {
		uint_ec(pAttributeHead->type);
		uint_ec(pAttributeHead->length);
		ushort_ec(pAttributeHead->nameOffset);
		ushort_ec(pAttributeHead->flags);
		ushort_ec(pAttributeHead->attributeId);
	}

	return true;
}

bool ReadAttribute(HANDLE hVolume, PBOOT_SECTOR pBootSector, PRESIDENT_ATTRIBUTE pAttribute, unsigned long long offset) {
	if (!ReadAttributeHead(hVolume, pBootSector, &pAttribute->attributeHead, offset))
		return false;
	if (pAttribute->attributeHead.nonResident)
		return false;

	if (!ReadVolumeOffset(hVolume, pBootSector, ((unsigned char*)pAttribute) + sizeof(ATTRIBUTE_HEAD), sizeof(RESIDENT_ATTRIBUTE) - sizeof(ATTRIBUTE_HEAD), offset + sizeof(ATTRIBUTE_HEAD)))
		return false;

	if (needToConvert) {
		uint_ec(pAttribute->dataLength);
		ushort_ec(pAttribute->dataOffset);
	}

	return true;
}

bool ReadAttribute(HANDLE hVolume, PBOOT_SECTOR pBootSector, PNONRESIDENT_ATTRIBUTE pAttribute, unsigned long long offset) {
	if (!ReadAttributeHead(hVolume, pBootSector, &pAttribute->attributeHead, offset))
		return false;
	if (!pAttribute->attributeHead.nonResident)
		return false;

	if (!ReadVolumeOffset(hVolume, pBootSector, ((unsigned char*)pAttribute) + sizeof(ATTRIBUTE_HEAD), sizeof(NONRESIDENT_ATTRIBUTE) - sizeof(ATTRIBUTE_HEAD), offset + sizeof(ATTRIBUTE_HEAD)))
		return false;

	if (needToConvert) {
		ull_ec(pAttribute->startVcn);
		ull_ec(pAttribute->lastVcn);
		ushort_ec(pAttribute->runArrayOffset);
		ushort_ec(pAttribute->compressionUnit);
		ull_ec(pAttribute->size);
		ull_ec(pAttribute->allocatedSize);
		ull_ec(pAttribute->unkownMeaning);
	}

	return true;
}

unsigned long long ReadRunListEntry(HANDLE hVolume, PBOOT_SECTOR pBootSector, PRUNLIST_ENTRY_INFO pEntryInfo, unsigned long long offset) {
	CUcharBuffer buffer(1 + sizeof(RUNLIST_ENTRY_INFO));
	if (!ReadVolumeOffset(hVolume, pBootSector, buffer, offset))
		return 0ULL;

	unsigned char lengths[] = decompress_uchar(buffer[0]);
	pEntryInfo->clusterTaken = pEntryInfo->startVcn = 0ULL;
	memcpy_s(&pEntryInfo->clusterTaken, sizeof(pEntryInfo->clusterTaken), buffer + 1, lengths[1]);
	memcpy_s(&pEntryInfo->startVcn, sizeof(pEntryInfo->startVcn), buffer + 1 + lengths[1], lengths[0]);

	if (needToConvert) {
		ull_ec(pEntryInfo->clusterTaken);
		ull_ec(pEntryInfo->startVcn);
	}

	return offset + 1 + lengths[0] + lengths[1];
}