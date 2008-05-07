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

#ifndef SHAPEMODEL_H
#define SHAPEMODEL_H

#include <QAbstractTableModel>

#include "verticesmodel.h"

class MaterialsModel;

typedef struct {
  quint8            type;
  quint8            depthIndex;
  VerticesModel*    verticesModel;
  MaterialsModel*   materialsModel;
  quint32           unknown1;
  quint32           unknown2;
} Primitive;

typedef QList<Primitive> PrimitivesList;

class ShapeModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  ShapeModel(QObject* parent = 0);

  Qt::ItemFlags     flags(const QModelIndex& index) const;
  QVariant          data(const QModelIndex& index, int role) const;
  bool              setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
  QVariant          headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

  bool              removeRows(int position, int rows, const QModelIndex& index = QModelIndex());

  int               rowCount(const QModelIndex& /*parent*/ = QModelIndex()) const    { return primitives.size(); }
  int               columnCount(const QModelIndex& /*parent*/ = QModelIndex()) const { return 4; }

  void              setShape(PrimitivesList& primitives);
  PrimitivesList*   primitivesList()                                                 { return &primitives; }
  Vertex*           boundBox();

public slots:
  void              removeRows(const QModelIndexList& rows);

private:
  PrimitivesList    primitives;
  Vertex            bound[8];

  static const int  DEPTH_MIN = 0;
  static const int  DEPTH_MAX = 255;
};

#endif
