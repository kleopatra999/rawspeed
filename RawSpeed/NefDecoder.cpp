#include "StdAfx.h"
#include "NefDecoder.h"

NefDecoder::NefDecoder(TiffIFD *rootIFD, FileMap* file ) : 
RawDecoder(file), mRootIFD(rootIFD)
{

}

NefDecoder::~NefDecoder(void)
{
}

RawImage NefDecoder::decodeRaw()
{
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(CFAPATTERN);

  if (data.empty())
    ThrowRDE("NEF Decoder: No image data found");

  TiffIFD* raw = data[0];
  int compression = raw->getEntry(COMPRESSION)->getInt();

  data = mRootIFD->getIFDsWithTag(MODEL);

  if (data.empty())
    ThrowRDE("NEF Decoder: No model data found");

  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (!data[0]->getEntry(MODEL)->getString().compare("NIKON D100 ")) {
    if (!D100IsCompressed(offsets->getInt()))
      compression = 1;
  }

  if (compression==1) {
    DecodeUncompressed();
    return mRaw;
  }

  if (offsets->count != 1) {
    ThrowRDE("NEF Decoder: Multiple Strips found: %u",offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE("NEF Decoder: Byte count number does not match strip size: count:%u, strips:%u ",counts->count, offsets->count);
  }
  if (!mFile->isValid(offsets->getInt()+counts->getInt()))
    ThrowRDE("NEF Decoder: Invalid strip byte count. File probably truncated.");


  if (34713 != compression)
    ThrowRDE("NEF Decoder: Unsupported compression");

  guint width = raw->getEntry(IMAGEWIDTH)->getInt();
  guint height = raw->getEntry(IMAGELENGTH)->getInt();
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();

  mRaw->dim = iPoint2D(width, height);
  mRaw->bpp = 2;
  mRaw->createData();

  data = mRootIFD->getIFDsWithTag(MAKERNOTE);
  if (data.empty())
    ThrowRDE("NEF Decoder: No EXIF data found");

  TiffIFD* exif = data[0];
  TiffEntry *makernoteEntry = exif->getEntry(MAKERNOTE);
  const guchar* makernote = makernoteEntry->getData();
  FileMap makermap((guchar*)&makernote[10], makernoteEntry->count-10);
  TiffParser makertiff(&makermap);
  makertiff.parseData();
  
  data = makertiff.RootIFD()->getIFDsWithTag((TiffTag)0x8c);

  if (data.empty())
    ThrowRDE("NEF Decoder: Decompression info tag not found");

  TiffEntry *meta;
  try {
    meta = data[0]->getEntry((TiffTag)0x96);
  } catch (TiffParserException) {
    meta = data[0]->getEntry((TiffTag)0x8c);  // Fall back
  }
 
  ByteStream metadata(meta->getData(), meta->count);
  
  NikonDecompressor decompressor(mFile,mRaw);
  decompressor.DecompressNikon(metadata, width, height, bitPerPixel,offsets->getInt(),counts->getInt());

  return mRaw;
}

/*
Figure out if a NEF file is compressed.  These fancy heuristics
are only needed for the D100, thanks to a bug in some cameras
that tags all images as "compressed".
*/
gboolean NefDecoder::D100IsCompressed(guint offset)
{
  const guchar *test = mFile->getData(offset);
  gint i;

  for (i=15; i < 1024; i+=16)
    if (test[i]) return true;
  return false;
} 

void NefDecoder::DecodeUncompressed() {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(CFAPATTERN);
  TiffIFD* raw = data[0];
  guint nslices = raw->getEntry(STRIPOFFSETS)->count;
  const guint *offsets = raw->getEntry(STRIPOFFSETS)->getIntArray();
  const guint *counts = raw->getEntry(STRIPBYTECOUNTS)->getIntArray();
  guint yPerSlice = raw->getEntry(ROWSPERSTRIP)->getInt();
  guint width = raw->getEntry(IMAGEWIDTH)->getInt();
  guint height = raw->getEntry(IMAGELENGTH)->getInt();
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();

  vector<NefSlice> slices;
  guint offY = 0;

  for (guint s = 0; s<nslices; s++) {
    NefSlice slice;
    slice.offset = offsets[s];
    slice.count = counts[s];
    if (offY+yPerSlice>height)
      slice.h = height-offY;
    else
      slice.h = yPerSlice;

    offY +=yPerSlice;

    if (mFile->isValid(slice.offset+slice.count)) // Only decode if size is valid
      slices.push_back(slice);
  }

  mRaw->dim = iPoint2D(width, offY);
  mRaw->bpp = 2;
  mRaw->createData();

  offY = 0;
  for (int i = 0; i< slices.size(); i++) {
    NefSlice slice = slices[i];
    ByteStream in(mFile->getData(slice.offset),slice.count);
    iPoint2D size(width,slice.h);
    iPoint2D pos(0,offY);
    readUncompressedRaw(in,size,pos,width*bitPerPixel/8,bitPerPixel,false);
    offY += slice.h;
}

}