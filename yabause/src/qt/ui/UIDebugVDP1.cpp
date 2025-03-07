/*	Copyright 2012 Theo Berkau <cwx@cyberwarriorx.com>

	This file is part of Yabause.

	Yabause is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Yabause is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Yabause; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include "UIDebugVDP1.h"
#include "CommonDialogs.h"

#include <QImageWriter>
#include <QGraphicsPixmapItem>
#include <sstream>


namespace {


struct Vdp1CommandsCount 
{
  size_t distortedSprites; 
  size_t polygons;
  size_t polylines;
  size_t normalSprites;
  size_t scaledSprites;
  size_t lines;
};

//
// Bresenham implementation from http://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm#C
//
void drawLine(uint32_t color, uint32_t* buffer, int width, int height,
  int x0, int y0, int x1, int y1)
{
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2, e2;
 
  for(;;)
  {
    int writeX = x0;
    int writeY = y0;
    if ( writeX < 0 )
      writeX = 0;
    else if ( writeX >= width )
      writeX = width - 1;

    if ( writeY < 0 )
      writeY = 0;
    else if ( writeY >= height )
      writeY = height - 1;
      
    buffer[writeY * width + writeX] = color;
    if (x0 == x1 && y0 == y1)
      break;

    e2 = err;
    if (e2 > -dx)
    {
      err -= dy;
      x0 += sx;
    }

    if (e2 < dy)
    {
      err += dx;
      y0 += sy;
    }
  }
}
         
void Vdp1CountCommands(u32 index, Vdp1CommandsCount& cmdCount) 
{
  Vdp1CommandType commandType = Vdp1DebugGetCommandType(index);
  switch (commandType) {
  case VDPCT_DISTORTED_SPRITE:
  case VDPCT_DISTORTED_SPRITEN:
    cmdCount.distortedSprites++;
    break;
  case VDPCT_NORMAL_SPRITE:
    cmdCount.normalSprites++;
    break;
  case VDPCT_SCALED_SPRITE:
    cmdCount.scaledSprites++;
    break;
  case VDPCT_POLYGON:
    cmdCount.polygons++;
    break;
  case VDPCT_POLYLINE:
  case VDPCT_POLYLINEN:
    cmdCount.polylines++;
    break;
  case VDPCT_LINE:
    cmdCount.lines++;
    break;
  }
}

std::string buildInfoLabel(Vdp1CommandsCount& cmdCount)
{
  bool previous = false;
  std::stringstream infoLabel;

  if (cmdCount.distortedSprites > 0) {
    infoLabel << "Distorted Sprites: " << cmdCount.distortedSprites;
    previous = true;
  }

  if (cmdCount.polygons > 0) {
    infoLabel << (previous ? ", " : "") << "Polygons: " << cmdCount.polygons;
    previous = true;
  }

  if (cmdCount.polylines > 0) {
    infoLabel << (previous ? ", " : "") << "PolyLines: " << 
      cmdCount.polylines;
    previous = true;
  }

  if (cmdCount.normalSprites > 0) {
    infoLabel << (previous ? ", " : "") << "Normal Sprites: " << 
      cmdCount.normalSprites;
    previous = true;
  }

  if (cmdCount.scaledSprites > 0) {
    infoLabel << (previous ? ", " : "") << "Scaled Sprites: " << 
      cmdCount.scaledSprites;
    previous = true;
  }

  if (cmdCount.lines > 0) {
    infoLabel << (previous ? ", " : "") << "Lines: " << 
      cmdCount.lines;
    previous = true;
  }

  return infoLabel.str();
}


} // namespace ''


UIDebugVDP1::UIDebugVDP1( QWidget* p, QWidget* glWidget )
	: QDialog( p )
{
	// setup dialog
	setupUi( this );
  yabauseGL = dynamic_cast<YabauseGL*>(glWidget);

  QGraphicsScene *scene=new QGraphicsScene(this);
  gvTexture->setScene(scene);

  lwCommandList->clear();

  Vdp1CommandsCount cmdCount;
  memset(&cmdCount, 0, sizeof(Vdp1CommandsCount));

  if (Vdp1Ram)
  {
     for (int i=0;;i++)
     {
        char outstring[256];

        Vdp1DebugGetCommandNumberName(i, outstring);
        if (*outstring == '\0')
           break;

        Vdp1CountCommands(i, cmdCount);
        lwCommandList->addItem(QtYabause::translate(outstring));
     }
  }

  vdp1texture = NULL;
  vdp1RawTexture = NULL;
  vdp1RawNumBytes = 0;
  vdp1texturew = vdp1textureh = 1;
  pbSaveBitmap->setEnabled(vdp1texture ? true : false);
  pbSaveRawSprite->setEnabled(vdp1RawTexture ? true : false);

  QString infoLabelText(QString::fromStdString(buildInfoLabel(cmdCount)));
  lVDP1Info->setText(infoLabelText);
  lVDP1Info->setToolTip(infoLabelText);

	// retranslate widgets
	QtYabause::retranslateWidget( this );
}

UIDebugVDP1::~UIDebugVDP1()
{
   if (vdp1texture)
      free(vdp1texture);

   if (vdp1RawTexture)
      free(vdp1RawTexture);
}

void UIDebugVDP1::on_lwCommandList_itemSelectionChanged ()
{
   int cursel = lwCommandList->currentRow();
   char tempstr[1024];

   Vdp1DebugCommand(cursel, tempstr);
   pteCommandInfo->clear();
   pteCommandInfo->appendPlainText(QtYabause::translate(tempstr));
   pteCommandInfo->moveCursor(QTextCursor::Start);

   if (vdp1texture)
      free(vdp1texture);

   if (vdp1RawTexture)
      free(vdp1RawTexture);

   vdp1texture = Vdp1DebugTexture(cursel, &vdp1texturew, &vdp1textureh);
   vdp1RawTexture = Vdp1DebugRawTexture(cursel, &vdp1texturew, &vdp1textureh, &vdp1RawNumBytes);

   pbSaveBitmap->setEnabled(vdp1texture ? true : false);
   pbSaveRawSprite->setEnabled(vdp1RawTexture ? true : false);

   // Redraw texture
   QGraphicsScene *scene = gvTexture->scene();
   QImage img((uchar *)vdp1texture, vdp1texturew, vdp1textureh, QImage::Format_ARGB32);
   QPixmap pixmap = QPixmap::fromImage(img.rgbSwapped());
   scene->clear();
   scene->addPixmap(pixmap);
   scene->setSceneRect(scene->itemsBoundingRect());
   gvTexture->fitInView(scene->sceneRect());
   gvTexture->invalidateScene();

   // Paint overlay if possible
   if (yabauseGL != nullptr)
   {
     int width, height, interlace;
     VIDCore->GetNativeResolution(&width, &height, &interlace);

     auto overlayPair = yabauseGL->getOverlayPausedImage();
     QMutexLocker lock(overlayPair.second);
     
     uint32_t* pauseImage = overlayPair.first;
     memset( pauseImage, 0, width * height * sizeof(uint32_t) );

     int x0, y0, x1, y1, x2, y2, x3, y3;
     if (Vdp1DebugCommandVertices(cursel, &x0, &y0, &x1, &y1, &x2, &y2, &x3, &y3) == 0)
     {
       const uint32_t color = 0xff0000dd;
       drawLine(color, pauseImage, width, height, x0, y0, x1, y1);
       drawLine(color, pauseImage, width, height, x1, y1, x2, y2);
       drawLine(color, pauseImage, width, height, x2, y2, x3, y3);
       drawLine(color, pauseImage, width, height, x3, y3, x0, y0);

       lock.unlock();
       yabauseGL->repaint();
       yabauseGL->updatePausedView(yabauseGL->size());
     }
   }
}

void UIDebugVDP1::on_pbSaveBitmap_clicked ()
{
	QStringList filters;
	foreach ( QByteArray ba, QImageWriter::supportedImageFormats() )
		if ( !filters.contains( ba, Qt::CaseInsensitive ) )
			filters << QString( ba ).toLower();
	for ( int i = 0; i < filters.count(); i++ )
		filters[i] = QtYabause::translate( "%1 Images (*.%2)" ).arg( filters[i].toUpper() ).arg( filters[i] );
	
	// take screenshot of gl view
   QImage img((uchar *)vdp1texture, vdp1texturew, vdp1textureh, QImage::Format_ARGB32);
   img = img.rgbSwapped();
	
	// request a file to save to to user
	const QString s = CommonDialogs::getSaveFileName( QString(), QtYabause::translate( "Choose a location for your bitmap" ), filters.join( ";;" ) );
	
	// write image if ok
	if ( !s.isEmpty() )
		if ( !img.save( s ) )
			CommonDialogs::information( QtYabause::translate( "An error occured while writing file." ) );
}

void UIDebugVDP1::on_pbSaveRawSprite_clicked ()
{
	QStringList filters( QString::fromUtf8( "*.bin" ) );
	
	// request a file to save to to user
	const QString answer = CommonDialogs::getSaveFileName( QString(), 
    QtYabause::translate( "Choose a location for your raw data" ), filters.join( ";;" ) );
	
	// write image if ok
	if ( !answer.isEmpty() ) {
    bool fileWritten = false;

    QFile outputFile(answer);
    if (outputFile.open(QIODevice::OpenModeFlag::WriteOnly | QIODevice::OpenModeFlag::Unbuffered))
    {
      if (outputFile.write(reinterpret_cast<const char*>(vdp1RawTexture), vdp1RawNumBytes) == vdp1RawNumBytes)
        fileWritten = true; 

      outputFile.close();
    }

		if (!fileWritten)
			CommonDialogs::information( QtYabause::translate( "An error occured while writing file." ) );
  }
}
