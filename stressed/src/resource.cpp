// stressed - Stunts/4D [Sports] Driving resource editor
// Copyright (C) 2008 Daniel Stien <daniel@stien.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QListWidget>

#include "bitmapresource.h"
#include "resource.h"
#include "settings.h"
#include "stunpack.h"
#include "textresource.h"

Resource::Resource(QString id, QWidget* parent, Qt::WFlags flags) :
  QWidget(parent, flags),
  id(id)
{
}

ResMap Resource::parse(const QString& fileName, QListWidget* idsList)
{
  ResMap resources;

  stpk_Buffer compSrc, compDst;
  compSrc.data = compDst.data = NULL;
  QBuffer buf;

  QString* ids = NULL;
  quint32* offsets = NULL;
  quint32 baseOffset;

  QFile file(fileName);

  if (!file.open(QIODevice::ReadOnly)) {
    throw tr("Couldn't open file for reading.");
  }

  QDataStream in(&file);
  in.setByteOrder(QDataStream::LittleEndian);

  try {
    quint64 fileSize = file.size(), actualSize;
    quint32 reportedSize;
    in >> reportedSize;

    // Not a valid resource file, try decompression.
    if (reportedSize != fileSize) {
      quint8 compType = reportedSize & STPK_PASSES_MASK;
      quint32 decompSize = reportedSize >> 8;

      if ((compType >= 1) && (compType <= 2) && (fileSize <= STPK_MAX_SIZE) && (fileSize < decompSize)) {
        compSrc.len = fileSize;
        compSrc.offset = compDst.offset = 0;

        try {
          compSrc.data = new uchar[compSrc.len];
        }
        catch (std::bad_alloc& exc) {
          compSrc.data = NULL;
          throw tr("Couldn't allocate memory for compressed file.");
        }

        file.seek(0);
        if (file.read((char*)compSrc.data, compSrc.len) != compSrc.len) {
          throw tr("Couldn't read compressed data to memory.");
        }

        char errStr[256];
        unsigned int res = stpk_decomp(&compSrc, &compDst, 0, 0, errStr);

        delete[] compSrc.data;
        compSrc.data = NULL;

        if (res) {
          errStr[255] = '\0';
          throw tr("Decompression failed with message \"%1\"").arg(errStr).simplified();
        }

        buf.setData((char*)compDst.data, compDst.len);
        buf.open(QIODevice::ReadOnly);
        in.setDevice(&buf);

        actualSize = compDst.len;
        delete[] compDst.data;
        compDst.data = NULL;
      }
      // Data doesn't fit compression header, give up.
      else {
        throw tr("Invalid file. Reported size (%1) doesn't match actual file size (%2) or compression header.").arg(reportedSize).arg(file.size());
      }
    }
    else {
      file.seek(0);
      actualSize = reportedSize;
    }

    // Decompression done, restart parsing.
    in >> reportedSize;

    if (reportedSize != actualSize) {
       throw tr("Invalid file. Reported size (%1) doesn't match actual file size (%2).").arg(reportedSize).arg(file.size());
    }

    quint16 numResources;
    in >> numResources;

    checkError(&in, tr("header"));

    // Table of contents.
    ids = new QString[numResources];

    char tmpId[5];
    tmpId[4] = '\0';

    for (int i = 0; i < numResources; i++) {
      in.readRawData(tmpId, 4);
      ids[i] = tmpId;
    }

    offsets = new quint32[numResources];
    for (int i = 0; i < numResources; i++) {
      in >> offsets[i];
    }

    checkError(&in, tr("table of contents"));

    // Resource data.
    baseOffset = in.device()->pos();

    StringMap types = Settings().getStringMap("types");

    for (int i = 0; i < numResources; i++) {
      in.device()->seek(baseOffset + offsets[i]);

      if (resources.count(ids[i])) {
        throw tr("Resource id \"%1\" not unique.").arg(ids[i]);
      }

      QString type = types[ids[i]];
      if (type == "text") {
        resources.insert(ids[i], new TextResource(ids[i], &in));
      }
      else if (type == "bitmap") {
        resources.insert(ids[i], new BitmapResource(ids[i], &in));
      }
      else {
        throw tr("Unknown type for id \"%1\"").arg(ids[i]);
      }

      // QHash entries are arbitrarily sorted, we're adding the id to the
      // list now to get the order matching the source file.
      idsList->addItem(ids[i]);
    }

    delete[] ids;
    delete[] offsets;

    in.unsetDevice();
    file.close();
  }
  catch (QString msg) {
    in.unsetDevice();
    file.close();

    delete[] compSrc.data;
    delete[] compDst.data;

    delete[] ids;
    delete[] offsets;

    throw msg;
  }

  return resources;
}

void Resource::write(const QString& fileName, const QListWidget* idsList, const ResMap& resources)
{
  QFile file(fileName);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    throw tr("Couldn't open file for writing.");
  }

  QDataStream out(&file);
  out.setByteOrder(QDataStream::LittleEndian);

  quint16 numResources = idsList->count();

  out << (quint32)0 << numResources;

  checkError(&out, tr("header"), true);

  for (int i = 0; i < numResources; i++) {
    QByteArray id = (QString("%1").arg(idsList->item(i)->text().left(4), 4, '\0')).toAscii();
    out << (qint8)id[0] << (qint8)id[1] << (qint8)id[2] << (qint8)id[3];
  }

  quint32 tocOffset = out.device()->size();

  // Placeholder offsets.
  for (int i = 0; i < numResources; i++) {
    out << (quint32)0;
  }

  checkError(&out, tr("table of contents"), true);

  quint32 curOffset = 0, baseOffset = out.device()->size();

  for (int i = 0; i < numResources; i++) {
    // Write offset in TOC.
    curOffset = out.device()->size() - baseOffset;
    out.device()->seek(tocOffset + (i * 4));
    out << curOffset;
    out.device()->seek(baseOffset + curOffset);

    // Write content.
    resources[idsList->item(i)->text()]->write(&out);
  }

  // Final file size header field.
  out.device()->seek(0);
  out << (quint32)out.device()->size();

  checkError(&out, tr("final file size"), true);

  out.unsetDevice();
  file.close();
}

void Resource::isModified()
{
  emit dataChanged();
}

void Resource::checkError(QDataStream* stream, const QString& what, bool write)
{
  switch (stream->status()) {
    case QDataStream::Ok:
      break;
    case QDataStream::ReadPastEnd:
      throw tr("Reached unexpected end of file while %1 %2.").arg(write ? "writing" : "reading").arg(what);
    case QDataStream::ReadCorruptData:
      throw tr("Data corruption occured while %1 %2.").arg(write ? "writing" : "reading").arg(what);
    default:
      throw tr("Device error occured while %1 %2 (\"%3\").").arg(write ? "writing" : "reading").arg(what).arg(stream->device()->errorString());
  }
}
