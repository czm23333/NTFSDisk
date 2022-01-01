#include "stdafx.h"
#include "NTFSDisk.h"
#include "Buffer.h"
#include <utility>

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

	return true;
}

unsigned long long ReadFileRecord(HANDLE hVolume, PBOOT_SECTOR pBootSector, PFILE_RECORD pFileRecord, unsigned long long sector) {
	if (!ReadVolumeSector(hVolume, pBootSector, pFileRecord, sizeof(FILE_RECORD), sector))
		return 0ULL;

	return sector * pBootSector->sectorInfo.bytesPerSector;
}

unsigned long long ReadFileRecordCluster(HANDLE hVolume, PBOOT_SECTOR pBootSector, PFILE_RECORD pFileRecord, unsigned long long cluster) {
	return ReadFileRecord(hVolume, pBootSector, pFileRecord, cluster * pBootSector->sectorInfo.sectorsPerCluster);
}

bool ReadAttributeHead(HANDLE hVolume, PBOOT_SECTOR pBootSector, PATTRIBUTE_HEAD pAttributeHead, unsigned long long offset) {
	if (!ReadVolumeOffset(hVolume, pBootSector, pAttributeHead, sizeof(ATTRIBUTE_HEAD), offset))
		return false;

	return true;
}

bool ReadAttribute(HANDLE hVolume, PBOOT_SECTOR pBootSector, PRESIDENT_ATTRIBUTE pAttribute, unsigned long long offset) {
	if (!ReadAttributeHead(hVolume, pBootSector, &pAttribute->attributeHead, offset))
		return false;
	if (pAttribute->attributeHead.nonResident)
		return false;

	if (!ReadVolumeOffset(hVolume, pBootSector, ((unsigned char*)pAttribute) + sizeof(ATTRIBUTE_HEAD), sizeof(RESIDENT_ATTRIBUTE) - sizeof(ATTRIBUTE_HEAD), offset + sizeof(ATTRIBUTE_HEAD)))
		return false;

	return true;
}

bool ReadAttribute(HANDLE hVolume, PBOOT_SECTOR pBootSector, PNONRESIDENT_ATTRIBUTE pAttribute, unsigned long long offset) {
	if (!ReadAttributeHead(hVolume, pBootSector, &pAttribute->attributeHead, offset))
		return false;
	if (!pAttribute->attributeHead.nonResident)
		return false;

	if (!ReadVolumeOffset(hVolume, pBootSector, ((unsigned char*)pAttribute) + sizeof(ATTRIBUTE_HEAD), sizeof(NONRESIDENT_ATTRIBUTE) - sizeof(ATTRIBUTE_HEAD), offset + sizeof(ATTRIBUTE_HEAD)))
		return false;

	return true;
}

unsigned long long ReadRunListEntry(HANDLE hVolume, PBOOT_SECTOR pBootSector, PRUNLIST_ENTRY_INFO pEntryInfo, unsigned long long offset) {
	CUcharBuffer buffer(1 + sizeof(RUNLIST_ENTRY_INFO));
	if (!ReadVolumeOffset(hVolume, pBootSector, buffer, offset))
		return 0ULL;

	unsigned char lengths[] = decompress_uchar(buffer[0]);
	pEntryInfo->clusterTaken = 0ULL;
	pEntryInfo->offsetLcn = 0LL;
	memcpy_s(&pEntryInfo->clusterTaken, sizeof(pEntryInfo->clusterTaken), buffer + 1, lengths[1]);
	if(buffer[lengths[1] + lengths[0]] & 0x80) pEntryInfo->offsetLcn = -1;
	memcpy_s(&pEntryInfo->offsetLcn, sizeof(pEntryInfo->offsetLcn), buffer + 1 + lengths[1], lengths[0]);

	return offset + 1 + lengths[0] + lengths[1];
}

unsigned long long ReadRunList(HANDLE hVolume, PBOOT_SECTOR pBootSector, PRUNLIST pRunList, unsigned long long offset) {
	pRunList->size = 0;
	RUNLIST_ENTRY_INFO entry;
	unsigned long long tmp;
	while ((tmp = ReadRunListEntry(hVolume, pBootSector, &entry, offset)) != offset + 1) {
		pRunList->entries.push_back(entry);
		pRunList->size += entry.clusterTaken * pBootSector->sectorInfo.sectorsPerCluster * pBootSector->sectorInfo.bytesPerSector;
		offset = tmp;
	}
	offset = tmp;

	return offset;
}

bool ReadRunListOffset(_In_ HANDLE hVolume, _In_ PRUNLIST pRunList, _In_ PBOOT_SECTOR pBootSector, _Out_ void* buffer, _In_ size_t size, _In_ unsigned long long offset) {
	long long tmp = offset, tmp1 = offset + size;
	long long curLcn = 0;
	unsigned char* buf = (unsigned char*)buffer;
	for (RUNLIST_ENTRY_INFO& info : pRunList->entries) {
		auto curBytes = info.clusterTaken * pBootSector->sectorInfo.sectorsPerCluster * pBootSector->sectorInfo.bytesPerSector;
		curLcn += info.offsetLcn;
		if (tmp < curBytes) {
			if (curBytes >= tmp1) {
				return ReadVolumeOffset(hVolume, pBootSector, buf, size, curLcn * pBootSector->sectorInfo.sectorsPerCluster * pBootSector->sectorInfo.bytesPerSector + tmp);
			}
			else {
				auto sz = long long(pBootSector->sectorInfo.sectorsPerCluster * pBootSector->sectorInfo.bytesPerSector) - tmp;
				if (!ReadVolumeOffset(hVolume, pBootSector, buf, sz, curLcn * pBootSector->sectorInfo.sectorsPerCluster * pBootSector->sectorInfo.bytesPerSector + tmp)) return false;
				buf += sz;
				tmp = 0;
				tmp1 -= curBytes;
			}
		}
		else {
			tmp -= curBytes;
			tmp1 -= curBytes;
		}
	}
	return false;
}

bool ReadRunListOffset(_In_ HANDLE hVolume, _In_ PRUNLIST pRunList, _In_ PBOOT_SECTOR pBootSector, _Out_ BUFFER buffer, _In_ unsigned long long offset) {
	return ReadRunListOffset(hVolume, pRunList, pBootSector, buffer.buffer, buffer.size, offset);
}

bool ReadRunListOffset(_In_ HANDLE hVolume, _In_ PRUNLIST pRunList, _In_ PBOOT_SECTOR pBootSector, _Out_ CUcharBuffer& buffer, _In_ unsigned long long offset) {
	return buffer.allocated && ReadRunListOffset(hVolume, pRunList, pBootSector, buffer.buffer, buffer.size, offset);
}

bool ReadRunListSector(_In_ HANDLE hVolume, _In_ PRUNLIST pRunList, _In_ PBOOT_SECTOR pBootSector, _Out_ void* buffer, _In_ size_t size, _In_ unsigned long long sector) {
	return ReadRunListOffset(hVolume, pRunList, pBootSector, buffer, size, sector * pBootSector->sectorInfo.bytesPerSector);
}

bool ReadRunListSector(_In_ HANDLE hVolume, _In_ PRUNLIST pRunList, _In_ PBOOT_SECTOR pBootSector, _Out_ BUFFER buffer, _In_ unsigned long long sector) {
	return ReadRunListSector(hVolume, pRunList, pBootSector, buffer.buffer, buffer.size, sector);
}

bool ReadRunListSector(_In_ HANDLE hVolume, _In_ PRUNLIST pRunList, _In_ PBOOT_SECTOR pBootSector, _Out_ CUcharBuffer& buffer, _In_ unsigned long long sector) {
	if (!buffer.allocated)
		buffer = std::move(CUcharBuffer(pBootSector->sectorInfo.bytesPerSector));
	return ReadRunListSector(hVolume, pRunList, pBootSector, buffer.buffer, buffer.size, sector);
}

bool ReadRunListCluster(_In_ HANDLE hVolume, _In_ PRUNLIST pRunList, _In_ PBOOT_SECTOR pBootSector, _Out_ void* buffer, _In_ size_t size, _In_ unsigned long long cluster) {
	return ReadRunListSector(hVolume, pRunList, pBootSector, buffer, size, cluster * pBootSector->sectorInfo.sectorsPerCluster);
}

bool ReadRunListCluster(_In_ HANDLE hVolume, _In_ PRUNLIST pRunList, _In_ PBOOT_SECTOR pBootSector, _Out_ BUFFER buffer, _In_ unsigned long long cluster) {
	return ReadRunListCluster(hVolume, pRunList, pBootSector, buffer.buffer, buffer.size, cluster);
}

bool ReadRunListCluster(_In_ HANDLE hVolume, _In_ PRUNLIST pRunList, _In_ PBOOT_SECTOR pBootSector, _Out_ CUcharBuffer& buffer, _In_ unsigned long long cluster) {
	if (!buffer.allocated)
		buffer = std::move(CUcharBuffer((size_t)pBootSector->sectorInfo.sectorsPerCluster * pBootSector->sectorInfo.bytesPerSector));
	return ReadRunListCluster(hVolume, pRunList, pBootSector, buffer.buffer, buffer.size, cluster);
}