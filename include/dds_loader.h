#pragma once

struct Mipmap
{
	int level;
	int width;
	int height;
	int size;
	unsigned char * data;
};

struct Face
{
	Mipmap * mipmaps;
	unsigned char * data;
};

struct DDSResult
{
	bool uncompressed;
	int mipmaps_count;

	int width;
	int height;

	unsigned int internal_format;
	unsigned int data_format;
	unsigned int data_type;

	Face faces[6];

	bool loaded = false;
};

char * DDSLoader_GetError();
DDSResult DDSLoader_Load(const char * filename);
void DDSLoader_Free(DDSResult & in);