#pragma once

#include "../include/dds_loader.h"
#include "image_helper.h"
#include "image_DXT.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GL_RGB				0x1907
#define GL_RGBA				0x1908
#define GL_UNSIGNED_BYTE	0x1401

#define RGB_DXT1		0x83F0
#define RGBA_DXT1		0x83F1
#define RGBA_DXT3		0x83F2
#define RGBA_DXT5		0x83F3

char * error_string = "success";
char * DDSLoader_GetError()
{
	return error_string;
}

void DDSLoader_DirectLoadFromMem(DDSResult & ret, unsigned char const * const buffer, int buffer_length)
{
	DDS_header header;
	unsigned int buffer_index = 0;
	unsigned int tex_ID = 0;

	unsigned int DDS_main_size, DDS_full_size;
	int cubemap, block_size = 16;
	unsigned int flag;
	int i;
	if (NULL == buffer)
	{
		error_string = "NULL buffer";
		return;
	}
	if (buffer_length < sizeof(DDS_header))
	{
		error_string = "DDS file was too small to contain the DDS header";
		return;
	}

	memcpy((void *)(&header), (const void *)buffer, sizeof(DDS_header));
	buffer_index = sizeof(DDS_header);

	flag = ('D' << 0) | ('D' << 8) | ('S' << 16) | (' ' << 24);
	if (header.dwMagic != flag) { return; }
	if (header.dwSize != 124) { return; }

	flag = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	if ((header.dwFlags & flag) != flag) { error_string = "Failed to read a known DDS header"; return; }

	flag = DDPF_FOURCC | DDPF_RGB;
	if ((header.sPixelFormat.dwFlags & flag) == 0) { error_string = "Failed to read a known DDS header"; return; }
	if (header.sPixelFormat.dwSize != 32) { error_string = "Failed to read a known DDS header"; return; }
	if ((header.sCaps.dwCaps1 & DDSCAPS_TEXTURE) == 0) { error_string = "Failed to read a known DDS header"; return; }

	if ((header.sPixelFormat.dwFlags & DDPF_FOURCC) &&
		!(
		(header.sPixelFormat.dwFourCC == (('D' << 0) | ('X' << 8) | ('T' << 16) | ('1' << 24))) ||
			(header.sPixelFormat.dwFourCC == (('D' << 0) | ('X' << 8) | ('T' << 16) | ('3' << 24))) ||
			(header.sPixelFormat.dwFourCC == (('D' << 0) | ('X' << 8) | ('T' << 16) | ('5' << 24)))
			))
	{
		error_string = "Failed to read a known DDS header";
		return;
	}

	ret.width = header.dwWidth;
	ret.height = header.dwHeight;
	ret.uncompressed = 1 - (header.sPixelFormat.dwFlags & DDPF_FOURCC) / DDPF_FOURCC;

	cubemap = (header.sCaps.dwCaps2 & DDSCAPS2_CUBEMAP) / DDSCAPS2_CUBEMAP;
	if (ret.uncompressed)
	{
		ret.internal_format = GL_RGB;
		block_size = 3;
		if (header.sPixelFormat.dwFlags & DDPF_ALPHAPIXELS)
		{
			ret.internal_format = GL_RGBA;
			block_size = 4;
		}
		DDS_main_size = ret.width * ret.height * block_size;
		ret.data_format = ret.internal_format;
		ret.data_type = GL_UNSIGNED_BYTE;
	}
	else
	{
		switch ((header.sPixelFormat.dwFourCC >> 24) - '0')
		{
			case 1:
				ret.internal_format = RGBA_DXT1;
				block_size = 8;
				break;
			case 3:
				ret.internal_format = RGBA_DXT3;
				block_size = 16;
				break;
			case 5:
				ret.internal_format = RGBA_DXT5;
				block_size = 16;
				break;
		}
		DDS_main_size = ((ret.width + 3) >> 2) * ((ret.height + 3) >> 2) * block_size;
		ret.data_format = DDS_main_size;
		ret.data_type = -1; // not needed
	}

	if ((header.sCaps.dwCaps1 & DDSCAPS_MIPMAP) && (header.dwMipMapCount > 1))
	{
		int shift_offset;
		ret.mipmaps_count = header.dwMipMapCount - 1;
		DDS_full_size = DDS_main_size;
		if (ret.uncompressed)
			shift_offset = 0;
		else
			shift_offset = 2;

		for (i = 1; i <= ret.mipmaps_count; ++i)
		{
			int w = ret.width >> (shift_offset + i);
			if (w < 1)
				w = 1;
			int h = ret.height >> (shift_offset + i);
			if (h < 1)
				h = 1;
			DDS_full_size += w * h * block_size;
		}
	}
	else
	{
		ret.mipmaps_count = 0;
		DDS_full_size = DDS_main_size;
	}

	for (int n = 0; n < 6; ++n)
	{
		ret.faces[n].data = (unsigned char *)malloc(DDS_full_size);
		ret.faces[n].mipmaps = new Mipmap[ret.mipmaps_count];

		if (buffer_index + DDS_full_size <= buffer_length)
		{
			unsigned int byte_offset = DDS_main_size;
			memcpy(ret.faces[n].data, (const void *)(&buffer[buffer_index]), DDS_full_size);
			buffer_index += DDS_full_size;
			if (ret.uncompressed)
			{
				for (i = 0; i < DDS_full_size; i += block_size)
				{
					unsigned char temp = ret.faces[n].data[i];
					ret.faces[n].data[i] = ret.faces[n].data[i + 2];
					ret.faces[n].data[i + 2] = temp;
				}
			}

			for (i = 1; i <= ret.mipmaps_count; ++i)
			{
				auto & mipmap = ret.faces[n].mipmaps[i-1];
				int w = ret.width >> i;
				if (w < 1)
					w = 1;

				int h = ret.height >> i;
				if (h < 1)
					h = 1;

				mipmap.level = i;
				mipmap.width = w;
				mipmap.height = h;
				mipmap.data = &ret.faces[n].data[byte_offset];

				if (ret.uncompressed)
					mipmap.size = w * h * block_size;
				else
					mipmap.size = ((w + 3) / 4) * ((h + 3) / 4) * block_size;

				byte_offset += mipmap.size;
			}
		}
		else
		{
			error_string = "DDS file was too small for expected image data";
			return;
		}
	}

	ret.loaded = true;
}

DDSResult DDSLoader_Load(const char * filename)
{
	DDSResult ret{};

	FILE * f;
	size_t buffer_length, bytes_read;
	unsigned char * buffer;

	if (NULL == filename)
	{
		error_string = "NULL filename";
		return ret;
	}
	f = fopen(filename, "rb");
	if (NULL == f)
	{
		error_string = "Can not find DDS file";
		return ret;
	}
	fseek(f, 0, SEEK_END);
	buffer_length = ftell(f);
	fseek(f, 0, SEEK_SET);
	buffer = (unsigned char *)malloc(buffer_length);

	if (NULL == buffer)
	{
		error_string = "malloc failed";
		fclose(f);
		return ret;
	}
	bytes_read = fread((void *)buffer, 1, buffer_length, f);
	fclose(f);
	if (bytes_read < buffer_length)
	{
		buffer_length = bytes_read;
	}

	DDSLoader_DirectLoadFromMem(ret, (unsigned char const * const)buffer, buffer_length);

	free(buffer);
	return ret;
}

void DDSLoader_Free(DDSResult & in)
{
	for (int n = 0; n < 6; ++n)
	{
		delete[] in.faces[n].mipmaps;
		free(in.faces[n].data);
	}
}