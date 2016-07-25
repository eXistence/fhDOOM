/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 2016 Johannes Ohlemacher

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "ImageData.h"
#include "ImageProgram.h"
#include "../framework/FileSystem.h"
#include "../framework/File.h"
#include "tr_local.h"

fhImageData::fhImageData()
	: data(nullptr)
	, format(pixelFormat_t::None)
	, numFaces(0)
	, numLevels(0)
	, timestamp(0) {
}

fhImageData::fhImageData(fhImageData&& other)
	: data( other.data )
	, format( other.format )
	, numFaces( other.numFaces )
	, numLevels( other.numLevels )
	, timestamp( other.timestamp ) {

	memcpy( &this->faces, &other.faces, sizeof( this->faces ) );

	other.data = nullptr;
	other.format = pixelFormat_t::None;
	other.numFaces = 0;
	other.numLevels = 0;
	other.timestamp = 0;
}

fhImageData::~fhImageData() {
	Clear();
}

fhImageData& fhImageData::operator=(fhImageData&& other) {
	Clear();

	this->data = other.data;
	this->format = other.format;
	this->numFaces = other.numFaces;
	this->numLevels = other.numLevels;
	this->timestamp = other.timestamp;

	memcpy( &this->faces, &other.faces, sizeof( this->faces ) );

	other.data = nullptr;
	other.format = pixelFormat_t::None;
	other.numFaces = 0;
	other.numLevels = 0;
	other.timestamp = 0;

	return *this;
}

void fhImageData::Clear() {
	if (data) {
		R_StaticFree(data);
		data = nullptr;
	}

	format = pixelFormat_t::None;
	numFaces = 0;
	numLevels = 0;
	timestamp = 0;

	assert(!IsValid());
}

bool fhImageData::IsValid() const {
	if (!data)
		return false;

	if (numFaces == 0)
		return false;

	if (numLevels == 0)
		return false;

	if (format == pixelFormat_t::None)
		return false;

	return true;
}

bool fhImageData::LoadRgbaFromMemory( const byte* pic, uint32 width, uint32 height ) {
	Clear();

	const int numBytes = width * height * 4;
	
	this->format = pixelFormat_t::RGBA;
	this->numFaces = 1;
	this->numLevels = 1;
	this->data = (byte*)R_StaticAlloc( numBytes );
	memcpy( this->data, pic, numBytes );

	auto& level = this->faces[0].levels[0];
	level.height = height;
	level.width = width;
	level.size = numBytes;
	level.offset = 0;

	return true;
}

bool fhImageData::LoadFile(const char* filename, bool toRgba /* = false */) {
	idStr name = filename;
	name.DefaultFileExtension(".tga");

	if (name.Length()<5) {
		return false;
	}

	name.ToLower();
	idStr ext;
	name.ExtractFileExtension(ext);

	bool ok = false;
	if (ext == "tga") {
		ok = LoadTGA(name.c_str(), toRgba);            // try tga first
		if (!ok) {
			name.StripFileExtension();
			name.DefaultFileExtension( ".jpg" );
			common->Warning( "jpg loading not implemented yet" );
			ok = false;
			//ok = LoadJPG( name.c_str(), pic, width, height, timestamp );
		}
	}	
	else if (ext == "dds") {
		if (toRgba) {
			common->Warning("Cannot convert compressed data to RGBA");
			return false;
		}
		ok = LoadDDS(name.c_str());
	}
	else if (ext == "pcx") {
		assert( false && "pcx loading not implemented yet" );
		ok = false;
		//ok = LoadPCX32( name.c_str(), pic, width, height, timestamp );
	}
	else if (ext == "bmp") {
		assert( false && "bmp loading not implemented yet" );
		ok = false;
		//ok = LoadBMP( name.c_str(), pic, width, height, timestamp );
	}
	else if (ext == "jpg") {
		assert( false && "jpg loading not implemented yet" );
		ok = false;
		//ok = LoadJPG( name.c_str(), pic, width, height, timestamp );
	}

	//
	// convert to exact power of 2 sizes
	//
	if (ok && this->data && this->numFaces == 1 && this->numLevels == 1 && this->format == pixelFormat_t::RGBA) {

		int	scaled_width, scaled_height;

		int w = GetWidth();
		int h = GetHeight();

		for (scaled_width = 1; scaled_width < w; scaled_width <<= 1)
			;
		for (scaled_height = 1; scaled_height < h; scaled_height <<= 1)
			;

		if (scaled_width != w || scaled_height != h) {
			if (globalImages->image_roundDown.GetBool() && scaled_width > w) {
				scaled_width >>= 1;
			}
			if (globalImages->image_roundDown.GetBool() && scaled_height > h) {
				scaled_height >>= 1;
			}

			byte* resampledBuffer = R_ResampleTexture(GetData(), w, h, scaled_width, scaled_height);
			R_StaticFree(this->data);
			this->data = resampledBuffer;			
			this->faces[0].levels[0].width = scaled_width;
			this->faces[0].levels[0].height = scaled_height;
			this->faces[0].levels[0].offset = 0;
			this->faces[0].levels[0].size = scaled_width * scaled_height * 4;
		}
	}

	return ok;
}

bool fhImageData::LoadDDS(const char* filename) {
	fhStaticBuffer<byte> buffer;
	if (LoadFileIntoBuffer(filename, buffer)) {
		return LoadDDS(buffer);
	}

	return false;
}

bool fhImageData::LoadTGA(const char* filename, bool toRgba) {
	fhStaticBuffer<byte> buffer;
	if (LoadFileIntoBuffer(filename, buffer)) {
		return LoadTGA(buffer, toRgba);
	}

	return false;
}

bool fhImageData::LoadDDS(fhStaticBuffer<byte>& buffer) {

	if (buffer.Num() < sizeof(ddsFileHeader_t)) {
		return false;
	}

	data = buffer.Release();

	unsigned long magic = LittleLong(*(unsigned long *)data);
	ddsFileHeader_t	*header = (ddsFileHeader_t *)(data + 4);

	// ( not byte swapping dwReserved1 dwReserved2 )
	header->dwSize = LittleLong(header->dwSize);
	header->dwFlags = LittleLong(header->dwFlags);
	header->dwHeight = LittleLong(header->dwHeight);
	header->dwWidth = LittleLong(header->dwWidth);
	header->dwPitchOrLinearSize = LittleLong(header->dwPitchOrLinearSize);
	header->dwDepth = LittleLong(header->dwDepth);
	header->dwMipMapCount = LittleLong(header->dwMipMapCount);
	header->dwCaps1 = LittleLong(header->dwCaps1);
	header->dwCaps2 = LittleLong(header->dwCaps2);

	header->ddspf.dwSize = LittleLong(header->ddspf.dwSize);
	header->ddspf.dwFlags = LittleLong(header->ddspf.dwFlags);
	header->ddspf.dwFourCC = LittleLong(header->ddspf.dwFourCC);
	header->ddspf.dwRGBBitCount = LittleLong(header->ddspf.dwRGBBitCount);
	header->ddspf.dwRBitMask = LittleLong(header->ddspf.dwRBitMask);
	header->ddspf.dwGBitMask = LittleLong(header->ddspf.dwGBitMask);
	header->ddspf.dwBBitMask = LittleLong(header->ddspf.dwBBitMask);
	header->ddspf.dwABitMask = LittleLong(header->ddspf.dwABitMask);

	const char* fourcc = (const char*)&header->ddspf.dwFourCC;

	if (header->ddspf.dwFlags & DDSF_FOURCC) {
		switch (header->ddspf.dwFourCC) {
		case DDS_MAKEFOURCC('D', 'X', 'T', '1'):
			if (header->ddspf.dwFlags & DDSF_ALPHAPIXELS) {
				format = pixelFormat_t::DXT1_RGBA;
			}
			else {
				format = pixelFormat_t::DXT1_RGB;
			}
			break;
		case DDS_MAKEFOURCC('D', 'X', 'T', '3'):
			format = pixelFormat_t::DXT3_RGBA;
			break;
		case DDS_MAKEFOURCC('D', 'X', 'T', '5'):
			format = pixelFormat_t::DXT5_RGBA;
			break;
		case DDS_MAKEFOURCC( 'A', 'T', 'I', '2' ):
			format = pixelFormat_t::RGTC;
			break;
		case DDS_MAKEFOURCC('R', 'X', 'G', 'B'):
			format = pixelFormat_t::DXT5_RxGB;
			break;
		default:
			common->Warning("Invalid compressed internal format\n");
			return false;
		}
	}
	else if ((header->ddspf.dwFlags & DDSF_RGBA) && header->ddspf.dwRGBBitCount == 32) {
		format = pixelFormat_t::BGRA;
	}
	else if ((header->ddspf.dwFlags & DDSF_RGB) && header->ddspf.dwRGBBitCount == 32) {
		format = pixelFormat_t::BGRA;
	}
	else if ((header->ddspf.dwFlags & DDSF_RGB) && header->ddspf.dwRGBBitCount == 24) {
		if (header->ddspf.dwFlags & DDSF_ID_INDEXCOLOR) {
			common->Warning( "Invalid uncompressed internal format\n" );
			return false;
		}
		else {
			format = pixelFormat_t::BGR;
		}
	}
	else if (header->ddspf.dwRGBBitCount == 8) {
		assert( false && "not supported" );
		common->Warning( "Invalid uncompressed internal format\n" );
		return false;
	}
	else {
		common->Warning( "Invalid uncompressed internal format\n" );
		return false;
	}

	this->numFaces = 1;
	this->numLevels = header->dwMipMapCount;

	byte *imagedata = this->data + sizeof(ddsFileHeader_t) + 4;
	int uw = header->dwWidth;
	int uh = header->dwHeight;

	for (uint32 i = 0; i < numLevels; i++) {
		int size;
		if ((int)format >= (int)pixelFormat_t::DXT1_RGB) {
			size = ((uw + 3) / 4) * ((uh + 3) / 4) * ((format == pixelFormat_t::DXT1_RGBA || format == pixelFormat_t::DXT1_RGB) ? 8 : 16);
		}
		else {
			size = uw * uh * (header->ddspf.dwRGBBitCount / 8);
		}

		faces[0].levels[i].offset = static_cast<uint32>((uintptr_t)imagedata - (uintptr_t)this->data);
		faces[0].levels[i].width = uw;
		faces[0].levels[i].height = uh;
		faces[0].levels[i].size = size;

		imagedata += size;
		uw /= 2;
		uh /= 2;
		if (uw < 1) {
			uw = 1;
		}
		if (uh < 1) {
			uh = 1;
		}
	}

	return true;
}

bool fhImageData::LoadTGA(fhStaticBuffer<byte>& buffer, bool toRgba) {

	struct TargaHeader {
		unsigned char 	id_length, colormap_type, image_type;
		unsigned short	colormap_index, colormap_length;
		unsigned char	colormap_size;
		unsigned short	x_origin, y_origin, width, height;
		unsigned char	pixel_size, attributes;
	};	

	if (buffer.Num() < sizeof(TargaHeader)) {
		return false;
	}

	int numBytes = 0;
	int	columns = 0;
	int rows = 0;
	int numPixels = 0;
	byte* buf_p = buffer.Get();

	TargaHeader	targa_header;
	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.colormap_length = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.y_origin = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.width = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.height = LittleShort(*(short *)buf_p);
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type != 2 && targa_header.image_type != 10 && targa_header.image_type != 3) {
		common->Error("LoadTGA( %s ): Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n", name);
	}

	if (targa_header.colormap_type != 0) {
		common->Error("LoadTGA( %s ): colormaps not supported\n", name);
	}

	if ((targa_header.pixel_size != 32 && targa_header.pixel_size != 24) && targa_header.image_type != 3) {
		common->Error("LoadTGA( %s ): Only 32 or 24 bit images supported (no colormaps)\n", name);
	}

	if (targa_header.image_type == 2 || targa_header.image_type == 3) {
		numBytes = targa_header.width * targa_header.height * (targa_header.pixel_size >> 3);
		if (numBytes > static_cast<int>(buffer.Num()) - 18 - targa_header.id_length) {
			common->Error("LoadTGA( %s ): incomplete file\n", name);
		}
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	level_t level;
	level.width = columns;
	level.height = rows;
	level.size = numPixels * 4;
	level.offset = 0;

	fhStaticBuffer<byte> rgba(numPixels * 4);

	if (targa_header.id_length != 0) {
		buf_p += targa_header.id_length;  // skip TARGA image comment
	}

	if (targa_header.image_type == 2 || targa_header.image_type == 3)
	{
		// Uncompressed RGB or gray scale image
		for (int row = rows - 1; row >= 0; row--)
		{
			byte* pixbuf = rgba.Get() + row*columns * 4;
			for (int column = 0; column < columns; column++)
			{
				unsigned char red, green, blue, alphabyte;
				switch (targa_header.pixel_size)
				{

				case 8:
					blue = *buf_p++;
					green = blue;
					red = blue;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;

				case 24:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alphabyte = *buf_p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				default:
					common->Error("LoadTGA( %s ): illegal pixel_size '%d'\n", name, targa_header.pixel_size);
					break;
				}
			}
		}
	}
	else if (targa_header.image_type == 10) {   // Runlength encoded RGB images
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;

		red = 0;
		green = 0;
		blue = 0;
		alphabyte = 0xff;

		for (int row = rows - 1; row >= 0; row--) {
			auto pixbuf = rgba.Get() + row*columns * 4;
			for (int column = 0; column < columns;) {
				packetHeader = *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {        // run-length packet
					switch (targa_header.pixel_size) {
					case 24:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = 255;
						break;
					case 32:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = *buf_p++;
						break;
					default:
						common->Error("LoadTGA( %s ): illegal pixel_size '%d'\n", name, targa_header.pixel_size);
						break;
					}

					for (j = 0; j < packetSize; j++) {
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if (column == columns) { // run spans across rows
							column = 0;
							if (row > 0) {
								row--;
							}
							else {
								goto breakOut;
							}
							pixbuf = rgba.Get() + row*columns * 4;
						}
					}
				}
				else {                            // non run-length packet
					for (j = 0; j < packetSize; j++) {
						switch (targa_header.pixel_size) {
						case 24:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
						case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						default:
							common->Error("LoadTGA( %s ): illegal pixel_size '%d'\n", name, targa_header.pixel_size);
							break;
						}
						column++;
						if (column == columns) { // pixel packet run spans across rows
							column = 0;
							if (row > 0) {
								row--;
							}
							else {
								goto breakOut;
							}
							pixbuf = rgba.Get() + row*columns * 4;
						}
					}
				}
			}
		breakOut:;
		}
	}

	if ((targa_header.attributes & (1 << 5))) {			// image flp bit
		R_VerticalFlip(rgba.Get(), level.width, level.height);
	}

	this->faces[0].levels[0] = level;
	this->numFaces = 1;
	this->numLevels = 1;
	this->data = rgba.Release();
	this->format = pixelFormat_t::RGBA;
	
	return true;
}

uint32 fhImageData::GetWidth(uint32 level) const {
	assert(numFaces > 0);
	assert(numLevels > level);
	return faces[0].levels[level].width;
}
uint32 fhImageData::GetHeight(uint32 level) const {
	assert(numFaces > 0);
	assert(numLevels > level);
	return faces[0].levels[level].height;
}

uint32 fhImageData::GetSize(uint32 level) const {
	assert(numFaces > 0);
	assert(numLevels > level);
	return faces[0].levels[level].size;
}

uint32 fhImageData::GetNumFaces() const {
	return numFaces;
}

uint32 fhImageData::GetNumLevels() const {
	return numLevels;
}

pixelFormat_t fhImageData::GetPixelFormat() const {
	return format;
}

ID_TIME_T fhImageData::GetTimeStamp() const {
	return timestamp;
}

const byte* fhImageData::GetData(uint32 face, uint32 level) const {
	if (face >= numFaces)
		return nullptr;

	if (level >= numLevels)
		return nullptr;

	return data + faces[face].levels[level].offset;
}

byte* fhImageData::GetData( uint32 face, uint32 level ) {
	if (face >= numFaces)
		return nullptr;

	if (level >= numLevels)
		return nullptr;

	return data + faces[face].levels[level].offset;
}

bool fhImageData::LoadFileIntoBuffer(const char* filename, fhStaticBuffer<byte>& buffer) {

	fhFileHandle file = fileSystem->OpenFileRead(filename);
	if (!file) {
		return false;
	}

	int	len = file->Length();

	ID_TIME_T time = file->Timestamp();
	if (time > this->timestamp) {
		timestamp = time;
	}

	strncpy(this->name, filename, Min(strlen(filename), sizeof(this->name) - 1));

	buffer.Free();
	buffer.Allocate(len);
	file->Read(buffer.Get(), len);
	file.Close();

	return true;
}

static const char* axisSides[] = { 
	"_px.tga", 
	"_nx.tga", 
	"_py.tga", 
	"_ny.tga",
	"_pz.tga", 
	"_nz.tga" 
};

static const char* cameraSides[] = {
	"_forward.tga",
	"_back.tga",
	"_left.tga",
	"_right.tga",
	"_up.tga",
	"_down.tga"
};

bool fhImageData::LoadCubeMap( const char* filename, cubeFiles_t cubeLayout ) {
	assert( cubeLayout == CF_CAMERA || cubeLayout == CF_NATIVE );

	const char	**sides = (cubeLayout == CF_CAMERA) ? cameraSides : axisSides;
	int	size = 0;
	int bytesPerSide = 0;
	ID_TIME_T timestamp = 0;
	pixelFormat_t format = pixelFormat_t::None;
	fhStaticBuffer<byte> buffer;

	Clear();
	
	for (int i = 0; i < 6; i++) {
		char	fullName[MAX_IMAGE_NAME];
		idStr::snPrintf( fullName, sizeof( fullName ), "%s%s", filename, sides[i] );

		fhImageData side;
		if (!side.LoadProgram( fullName )) {
			return false;
		}		

		if (i == 0) {
			timestamp = side.GetTimeStamp();
			size = side.GetWidth();
			bytesPerSide = size * size * 4; //4 bytes, due to RGBA
			format = side.GetPixelFormat();
			buffer.Allocate( bytesPerSide * 6 );
		}

		if (side.GetWidth() != size || side.GetHeight() != size) {
			common->Warning( "Mismatched sizes on cube map '%s'", fullName );
			return false;
		}

		if (side.GetPixelFormat() != format ) {
			common->Warning( "Mismatched pixel format on cube map '%s'", fullName );
			return false;
		}

		if (side.GetSize( 0 ) != bytesPerSide) {
			common->Warning( "Unexpected size of cube map side '%s'", fullName );
			return false;
		}

		if (cubeLayout == CF_CAMERA) {
			// convert from "camera" images to native cube map images
			switch (i) {
			case 0:	// forward
				R_RotatePic( side.GetData(), size );
				break;
			case 1:	// back
				R_RotatePic( side.GetData(), size );
				R_HorizontalFlip( side.GetData(), size, size );
				R_VerticalFlip( side.GetData(), size, size );
				break;
			case 2:	// left
				R_VerticalFlip( side.GetData(), size, size );
				break;
			case 3:	// right
				R_HorizontalFlip( side.GetData(), size, size );
				break;
			case 4:	// up
				R_RotatePic( side.GetData(), size );
				break;
			case 5: // down
				R_RotatePic( side.GetData(), size );
				break;
			}
		}

		memcpy( &buffer.Get()[i * bytesPerSide], side.GetData(), bytesPerSide );
	}

	this->data = buffer.Release();
	this->numFaces = 6;
	this->numLevels = 1;
	this->format = format;
	this->timestamp = timestamp;

	for (int i = 0; i < 6; ++i) {
		auto& level = this->faces[i].levels[0];
		level.width = size;
		level.height = size;
		level.offset = bytesPerSide * i;
		level.size = bytesPerSide;
	}

	return true;
}

bool fhImageData::LoadProgram( const char* program ) {
	Clear();

	fhImageProgram imageProgram;
	return imageProgram.LoadImageProgram( program, this, nullptr );
}