// -------------------------------------------------------------------------------------------------------------------
//
//  File: GraphicsWidget.cpp
//
//  Copyright 2016 (c) Decawave Ltd, Dublin, Ireland.
//
//  All rights reserved.
//
//  Author:
//
// -------------------------------------------------------------------------------------------------------------------

#include "GraphicsWidget.h"
#include "ui_GraphicsWidget.h"

#include "RTLSDisplayApplication.h"
#include "ViewSettings.h"

#include <QDomDocument>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsItemGroup>
#include <QGraphicsRectItem>
#include <QDebug>
#include <QInputDialog>
#include <QFile>
#include <QPen>
#include <QDesktopWidget>

#define PEN_WIDTH (0.04)
#define ANC_SIZE (0.15)
#define FONT_SIZE (10)

GraphicsWidget::GraphicsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GraphicsWidget)
{
    QDesktopWidget desktop;
    int desktopHeight=desktop.geometry().height();
    int desktopWidth=desktop.geometry().width();

    ui->setupUi(this);

    this->_scene = new QGraphicsScene(this);

    ui->graphicsView->setScene(this->_scene);
    ui->graphicsView->scale(1, -1);
    //ui->graphicsView->setBaseSize(this->height()*0.75, this->width());
    ui->graphicsView->setBaseSize(desktopHeight*0.75, desktopWidth*0.75);

    //tagTable
    //Tag ID, x, y, z, r95, vbat? ...
    QStringList tableHeader;
    tableHeader << "Tag ID/Label" << "X\n(m)" << "Y\n(m)" << "Z\n(m)" << "R95 \n(m)"
                << "Anc 0\n range (m)" << "Anc 1\n range (m)" << "Anc 2\n range (m)" << "Anc 3\n range (m)"
                   ;
    ui->tagTable->setHorizontalHeaderLabels(tableHeader);

    ui->tagTable->setColumnWidth(ColumnID,100);    //ID
    ui->tagTable->setColumnWidth(ColumnX,55); //x
    ui->tagTable->setColumnWidth(ColumnY,55); //y
    ui->tagTable->setColumnWidth(ColumnZ,55); //z
    //ui->tagTable->setColumnWidth(ColumnBlinkRx,70); //% Blinks RX
    //ui->tagTable->setColumnWidth(ColumnLocations,70); //% Multilaterations Success
    ui->tagTable->setColumnWidth(ColumnR95,70); //R95

    ui->tagTable->setColumnWidth(ColumnRA0,70);
    ui->tagTable->setColumnWidth(ColumnRA1,70);
    ui->tagTable->setColumnWidth(ColumnRA2,70);
    ui->tagTable->setColumnWidth(ColumnRA3,70);

    ui->tagTable->setColumnHidden(ColumnIDr, true); //ID raw hex
    //ui->tagTable->setColumnWidth(ColumnIDr,70); //ID raw hex

    if(desktopWidth <= 800)
    {
        ui->tagTable->setMaximumWidth(200);
    }

    //anchorTable
    //Anchor ID, x, y, z,
    QStringList anchorHeader;
    anchorHeader << "Anchor ID" << "X\n(m)" << "Y\n(m)" << "Z\n(m)"
                    << "T0\n(cm)" << "T1\n(cm)" << "T2\n(cm)" << "T3\n(cm)"
                    << "T4\n(cm)" << "T5\n(cm)" << "T6\n(cm)" << "T7\n(cm)" ;
    ui->anchorTable->setHorizontalHeaderLabels(anchorHeader);

    ui->anchorTable->setColumnWidth(ColumnID,100);    //ID
    ui->anchorTable->setColumnWidth(ColumnX,55); //x
    ui->anchorTable->setColumnWidth(ColumnY,55); //y
    ui->anchorTable->setColumnWidth(ColumnZ,55); //z
    ui->anchorTable->setColumnWidth(4,55); //t0
    ui->anchorTable->setColumnWidth(5,55); //t1
    ui->anchorTable->setColumnWidth(6,55); //t2
    ui->anchorTable->setColumnWidth(7,55); //t3
    ui->anchorTable->setColumnWidth(8,55); //t4
    ui->anchorTable->setColumnWidth(9,55); //t5
    ui->anchorTable->setColumnWidth(10,55); //t6
    ui->anchorTable->setColumnWidth(11,55); //t7

    //hide Anchor/Tag range corection table
    hideTACorrectionTable(true);

    ui->anchorTable->setMaximumHeight(120);
    ui->tagTable->setMaximumHeight(120);

    //set defaults
    _tagSize = 0.15;
    _historyLength = 20;
    _showHistoryP = _showHistory = true;

    _busy = true ;
    _ignore = true;

    _selectedTagIdx = -1;

    zone1 = NULL;
    zone2 = NULL;

    _maxRad = 1000;

    _minRad = 0;

    _zone1Rad = 0;
    _zone2Rad = 0;
    _zone1Red = false;
    _zone2Red = false;

    _geoFencingMode = false;
    _alarmOut = false;

    _line01 = NULL;
    _line02 = NULL;
    _line12 = NULL;
    RTLSDisplayApplication::connectReady(this, "onReady()");
}

void GraphicsWidget::onReady()
{
	QObject::connect(ui->tagTable, SIGNAL(cellChanged(int, int)), this, SLOT(tagTableChanged(int, int)));
    QObject::connect(ui->anchorTable, SIGNAL(cellChanged(int, int)), this, SLOT(anchorTableChanged(int, int)));
    QObject::connect(ui->tagTable, SIGNAL(cellClicked(int, int)), this, SLOT(tagTableClicked(int, int)));
    QObject::connect(ui->anchorTable, SIGNAL(cellClicked(int, int)), this, SLOT(anchorTableClicked(int, int)));
    QObject::connect(ui->tagTable, SIGNAL(itemSelectionChanged()), this, SLOT(itemSelectionChanged()));
    QObject::connect(ui->anchorTable, SIGNAL(itemSelectionChanged()), this, SLOT(itemSelectionChangedAnc()));

    QObject::connect(this, SIGNAL(centerAt(double,double)), graphicsView(), SLOT(centerAt(double, double)));
    QObject::connect(this, SIGNAL(centerRect(QRectF)), graphicsView(), SLOT(centerRect(QRectF)));

    _busy = false ;
}

GraphicsWidget::~GraphicsWidget()
{
    delete _scene;
    delete ui;
}

GraphicsView *GraphicsWidget::graphicsView()
{
    return ui->graphicsView;
}


void GraphicsWidget::loadConfigFile(QString filename)
{
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug(qPrintable(QString("Error: Cannot read file %1 %2").arg(filename).arg(file.errorString())));
        return;
    }

    QDomDocument doc;
    QString error;
    int errorLine;
    int errorColumn;

    if(doc.setContent(&file, false, &error, &errorLine, &errorColumn))
    {
        qDebug() << "file error !!!" << error << errorLine << errorColumn;

        emit setTagHistory(_historyLength);
    }

    QDomElement config = doc.documentElement();

    if( config.tagName() == "config" )
    {
        QDomNode n = config.firstChild();
        while( !n.isNull() )
        {
            QDomElement e = n.toElement();
            if( !e.isNull() )
            {
                if( e.tagName() == "tag_cfg" )
                {
                    _tagSize = (e.attribute( "size", "" )).toDouble();
                    _historyLength = (e.attribute( "history", "" )).toInt();
                }
                else
                if( e.tagName() == "tag" )
                {
                    bool ok;
                    quint64 id = (e.attribute( "ID", "" )).toULongLong(&ok, 16);
                    QString label = (e.attribute( "label", "" ));

                    _tagLabels.insert(id, label);
                }
            }

            n = n.nextSibling();
        }

    }

    file.close();

    emit setTagHistory(_historyLength);
}


void GraphicsWidget::hideTACorrectionTable(bool hidden)
{
    ui->anchorTable->setColumnHidden(4,hidden); //t0
    ui->anchorTable->setColumnHidden(5,hidden); //t1
    ui->anchorTable->setColumnHidden(6,hidden); //t2
    ui->anchorTable->setColumnHidden(7,hidden); //t3
    ui->anchorTable->setColumnHidden(8,hidden); //t4
    ui->anchorTable->setColumnHidden(9,hidden); //t5
    ui->anchorTable->setColumnHidden(10,hidden); //t6
    ui->anchorTable->setColumnHidden(11,hidden); //t7
}


QDomElement TagToNode( QDomDocument &d, quint64 id, QString label )
{
    QDomElement cn = d.createElement( "tag" );
    cn.setAttribute("ID", QString::number(id, 16));
    cn.setAttribute("label", label);
    return cn;
}

void GraphicsWidget::saveConfigFile(QString filename)
{
    QFile file( filename );

    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        qDebug(qPrintable(QString("Error: Cannot read file %1 %2").arg("./TREKtag_config.xml").arg(file.errorString())));
        return;
    }

    QDomDocument doc;

    // Adding tag config root
    QDomElement config = doc.createElement("config");
    doc.appendChild(config);

    QDomElement cn = doc.createElement( "tag_cfg" );
    cn.setAttribute("size", QString::number(_tagSize));
    cn.setAttribute("history", QString::number(_historyLength));
    config.appendChild(cn);

    //update the map
    QMap<quint64, QString>::iterator i = _tagLabels.begin();

    while (i != _tagLabels.end())
    {
        config.appendChild( TagToNode(doc, i.key(), i.value()) );

        i++;
    }

    QTextStream ts( &file );
    ts << doc.toString();

    file.close();

    qDebug() << doc.toString();
}

void GraphicsWidget::clearTags(void)
{
    qDebug() << "table rows " << ui->tagTable->rowCount() << " list " << this->_tags.size();

    while (ui->tagTable->rowCount())
    {
        QTableWidgetItem* item = ui->tagTable->item(0, ColumnIDr);

        if(item)
        {
            qDebug() << "Item text: " << item->text();

            bool ok;
            quint64 tagID = item->text().toULongLong(&ok, 16);
            //clear scene from any tags
            Tag *tag = this->_tags.value(tagID, NULL);
            if(tag->r95p) //remove R95
            {
                //re-size the elipse... with a new rad value...
                tag->r95p->setOpacity(0); //hide it

                this->_scene->removeItem(tag->r95p);
                delete(tag->r95p);
                tag->r95p = NULL;
            }
            if(tag->avgp) //remove average
            {
                //re-size the elipse... with a new rad value...
                tag->avgp->setOpacity(0); //hide it

                this->_scene->removeItem(tag->avgp);
                delete(tag->avgp);
                tag->avgp = NULL;
            }
            if(tag->geop) //remove average
            {
                //re-size the elipse... with a new rad value...
                tag->geop->setOpacity(0); //hide it

                this->_scene->removeItem(tag->geop);
                delete(tag->geop);
                tag->geop = NULL;
            }
            if(tag->tagLabel) //remove label
            {
                //re-size the elipse... with a new rad value...
                tag->tagLabel->setOpacity(0); //hide it

                this->_scene->removeItem(tag->tagLabel);
                delete(tag->tagLabel);
                tag->tagLabel = NULL;
            }

            //remove history...
            for(int idx=0; idx<_historyLength; idx++ )
            {
                QAbstractGraphicsShapeItem *tag_p = tag->p[idx];
                if(tag_p)
                {
                    tag_p->setOpacity(0); //hide it

                    this->_scene->removeItem(tag_p);
                    delete(tag_p);
                    tag_p = NULL;
                    tag->p[idx] = 0;

                    qDebug() << "hist remove tag " << idx;
                }
            }
            {
                QMap<quint64, Tag*>::iterator i = _tags.find(tagID);

                if(i != _tags.end()) _tags.erase(i);
            }
        }
        ui->tagTable->removeRow(0);
    }

    //clear tag table
    ui->tagTable->clearContents();

    qDebug() << "clear tags/tag table";

}

void GraphicsWidget::itemSelectionChanged(void)
{
    QList <QTableWidgetItem *>  l = ui->tagTable->selectedItems();
}

void GraphicsWidget::itemSelectionChangedAnc(void)
{
    QList <QTableWidgetItem *>  l = ui->anchorTable->selectedItems();
}

void GraphicsWidget::tagTableChanged(int r, int c)
{
    if(!_ignore)
    {
        Tag *tag = NULL;
        bool ok;
        quint64 tagId = (ui->tagTable->item(r,ColumnIDr)->text()).toULongLong(&ok, 16);
        tag = _tags.value(tagId, NULL);

        if(!tag) return;

        if(c == ColumnID) //label has changed
        {
            QString newLabel = ui->tagTable->item(r,ColumnID)->text();

            tag->tagLabelStr = newLabel;
            tag->tagLabel->setText(newLabel);

            //update the map
            QMap<quint64, QString>::iterator i = _tagLabels.find(tagId);

            if(i == _tagLabels.end()) //did not find the label
            {
                //insert the new value
                _tagLabels.insert(tagId, newLabel);
            }
            else //if (i != taglabels.end()) // && i.key() == tagId)
            {
                _tagLabels.erase(i); //erase the key
                _tagLabels.insert(tagId, newLabel);
            }
        }
    }
}

void GraphicsWidget::anchorTableChanged(int r, int c)
{
    if(!_ignore)
    {
        _ignore = true;

        Anchor *anc = NULL;
        bool ok;

        anc = _anchors.value(r, NULL);

        if(!anc) return;

        switch(c)
        {
            case ColumnX:
            case ColumnY:
            case ColumnZ:
            {
                double xyz = (ui->anchorTable->item(r,c)->text()).toDouble(&ok);
                if(ok)
                {
                    if((ColumnZ == c) && (r > 2))
                    {
                        emit updateAnchorXYZ(r, c, xyz);
                    }
                    else
                    {
                        emit updateAnchorXYZ(r, c, xyz);
                    }

                    double xn = (ui->anchorTable->item(r,ColumnX)->text()).toDouble(&ok);
                    double yn = (ui->anchorTable->item(r,ColumnY)->text()).toDouble(&ok);
                    double zn = (ui->anchorTable->item(r,ColumnZ)->text()).toDouble(&ok);

                    ui->anchorTable->item(r,ColumnX)->setText(QString::number(xn, 'f', 2));
                    ui->anchorTable->item(r,ColumnY)->setText(QString::number(yn, 'f', 2));

                    anchPos(r, xn, yn, zn, true, false);

                    if((ColumnZ == c) && (r <= 2))
                    {
                        //update z of 1, 2, and 3 to be the same
                        emit updateAnchorXYZ(0, c, xyz);
                        emit updateAnchorXYZ(1, c, xyz);
                        emit updateAnchorXYZ(2, c, xyz);

                        //make all 3 anchors have the same z coordinate
                        ui->anchorTable->item(0,ColumnZ)->setText(QString::number(zn, 'f', 2));
                        ui->anchorTable->item(1,ColumnZ)->setText(QString::number(zn, 'f', 2));
                        ui->anchorTable->item(2,ColumnZ)->setText(QString::number(zn, 'f', 2));
                    }
                }
            }
            break;
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            {
                int value = (ui->anchorTable->item(r,c)->text()).toInt(&ok);
                if(ok)
                    emit updateTagCorrection(r, c-4, value);
            }
            break;
        }

        _ignore = false;
    }
}

void GraphicsWidget::anchorTableClicked(int r, int c)
{
    Anchor *anc = NULL;

    anc = _anchors.value(r, NULL);

    if(!anc) return;

    if(c == ColumnID) //toggle label
    {
        QTableWidgetItem *pItem = ui->anchorTable->item(r, c);
        anc->show = (pItem->checkState() == Qt::Checked) ? true : false;

        anc->a->setOpacity(anc->show ? 1.0 : 0.0);
        anc->ancLabel->setOpacity(anc->show ? 1.0 : 0.0);
    }
}

void GraphicsWidget::tagTableClicked(int r, int c)
{
    Tag *tag = NULL;
    bool ok;
    quint64 tagId = (ui->tagTable->item(r,ColumnIDr)->text()).toULongLong(&ok, 16);
    tag = _tags.value(tagId, NULL);

    _selectedTagIdx = r;

    if(!tag) return;

    if(c == ColumnR95) //toggle R95 display
    {
        QTableWidgetItem *pItem = ui->tagTable->item(r, c);
        tag->r95Show = (pItem->checkState() == Qt::Checked) ? true : false;
    }

    if(c == ColumnID) //toggle label
    {
        QTableWidgetItem *pItem = ui->tagTable->item(r, c);
        tag->showLabel = (pItem->checkState() == Qt::Checked) ? true : false;

        tag->tagLabel->setOpacity(tag->showLabel ? 1.0 : 0.0);
    }

}

/**
 * @fn    tagIDtoString
 * @brief  convert hex Tag ID to string (preappend 0x)
 *
 * */
void GraphicsWidget::tagIDToString(quint64 tagId, QString *t)
{
    //NOTE: this needs to be hex string as it is written into ColumnIDr
    //and is used later to remove the correct Tag
    *t = "0x"+QString::number(tagId, 16);
}

int GraphicsWidget::findTagRowIndex(QString &t)
{
    for (int ridx = 0 ; ridx < ui->tagTable->rowCount() ; ridx++ )
    {
        QTableWidgetItem* item = ui->tagTable->item(ridx, ColumnIDr);

        if(item)
        {
            if(item->text() == t)
            {
                return ridx;
            }
        }
    }

    return -1;
}

/**
 * @fn    insertTag
 * @brief  insert Tag into the tagTable at row ridx
 *
 * */
void GraphicsWidget::insertTag(int ridx, QString &t, bool showR95, bool showLabel, QString &l)
{
    _ignore = true;

    ui->tagTable->insertRow(ridx);

    qDebug() << "Insert Tag" << ridx << t << l;

    for( int col = ColumnID ; col < ui->tagTable->columnCount(); col++)
    {
        QTableWidgetItem* item = new QTableWidgetItem();
        QTableWidgetItem *pItem = new QTableWidgetItem();
        if(col == ColumnID )
        {
            if(showLabel)
            {
                pItem->setCheckState(Qt::Checked);
                pItem->setText(l);
            }
            else
            {
                pItem->setCheckState(Qt::Unchecked);
                pItem->setText(l);
            }
            item->setFlags((item->flags() ^ Qt::ItemIsEditable) | Qt::ItemIsSelectable);
            //ui->tagTable->setItem(ridx, col, item);
            ui->tagTable->setItem(ridx, col, pItem);
        }
        else
        {
            if(col == ColumnIDr)
            {
                item->setText(t);
            }

            item->setFlags((item->flags() ^ Qt::ItemIsEditable) | Qt::ItemIsSelectable);
            ui->tagTable->setItem(ridx, col, item);
        }

        if(col == ColumnR95)
        {
            if(showR95)
            {
                pItem->setCheckState(Qt::Checked);
            }
            else
            {
                pItem->setCheckState(Qt::Unchecked);
            }
            pItem->setText(" ");
            ui->tagTable->setItem(ridx,col,pItem);
        }
   }

   _ignore = false; //we've added a row
}

/**
 * @fn    insertAnchor
 * @brief  insert Anchor/Tag correction values into the anchorTable at row ridx
 *
 * */
void GraphicsWidget::insertAnchor(int ridx, double x, double y, double z,int *array, bool show)
{
    int i = 0;
    _ignore = true;

    //add tag offsets
    for( int col = 4 ; col < ui->anchorTable->columnCount(); col++)
    {
        QTableWidgetItem* item = new QTableWidgetItem();
        item->setText(QString::number(array[i]));
        item->setTextAlignment(Qt::AlignHCenter);
        //item->setFlags((item->flags() ^ Qt::ItemIsEditable) | Qt::ItemIsSelectable);
        ui->anchorTable->setItem(ridx, col, item);
    }

    //add x, y, z
    QTableWidgetItem* itemx = new QTableWidgetItem();
    QTableWidgetItem* itemy = new QTableWidgetItem();
    QTableWidgetItem* itemz = new QTableWidgetItem();

    itemx->setText(QString::number(x, 'f', 2));
    itemy->setText(QString::number(y, 'f', 2));
    itemz->setText(QString::number(z, 'f', 2));

    ui->anchorTable->setItem(ridx, ColumnX, itemx);
    ui->anchorTable->setItem(ridx, ColumnY, itemy);
    ui->anchorTable->setItem(ridx, ColumnZ, itemz);

    {
        QTableWidgetItem *pItem = new QTableWidgetItem();

        if(show)
        {
            pItem->setCheckState(Qt::Checked);
        }
        else
        {
            pItem->setCheckState(Qt::Unchecked);
        }
        pItem->setText(QString::number(ridx));
        pItem->setTextAlignment(Qt::AlignHCenter);

        pItem->setFlags((pItem->flags() ^ Qt::ItemIsEditable) | Qt::ItemIsSelectable);

        ui->anchorTable->setItem(ridx, ColumnID, pItem);
    }

    ui->anchorTable->showRow(ridx);
    _ignore = false;
}

/**
 * @fn    addNewTag
 * @brief  add new Tag with tagId into the tags QMap
 *
 * */
void GraphicsWidget::addNewTag(quint64 tagId)
{
    static double c_h = 0.1;
    Tag *tag;
    int tid = tagId ;
    QString taglabel = QString("Tag %1").arg(tid); //taglabels.value(tagId, NULL);

    qDebug() << "Add new Tag: 0x" + QString::number(tagId, 16) << tagId;

    //insert into QMap list, and create an array to hold history of its positions
    _tags.insert(tagId,new(Tag));

    tag = this->_tags.value(tagId, NULL);
    tag->p.resize(_historyLength);

    c_h += 0.568034;
    if (c_h >= 1)
        c_h -= 1;
    tag->colourH = c_h;
    tag->colourS = 0.55;
    tag->colourV = 0.98;

    tag->tsPrev = 0;
    tag->r95Show = false;
    tag->showLabel = (taglabel != NULL) ? true : false;
    tag->tagLabelStr = taglabel;
    //add ellipse for the R95 - set to transparent until we get proper r95 data
    tag->r95p = this->_scene->addEllipse(-0.1, -0.1, 0, 0);
    tag->r95p->setOpacity(0);
    tag->r95p->setPen(Qt::NoPen);
    tag->r95p->setBrush(Qt::NoBrush);
    //add ellipse for average point - set to transparent until we get proper average data
    tag->avgp = this->_scene->addEllipse(0, 0, 0, 0);
    tag->avgp->setBrush(Qt::NoBrush);
    tag->avgp->setPen(Qt::NoPen);
    tag->avgp->setOpacity(0);
    //add ellipse for geo fencing point - set to transparent until we get proper average data
    tag->geop = this->_scene->addEllipse(0, 0, 0, 0);
    tag->geop->setBrush(Qt::NoBrush);
    tag->geop->setPen(Qt::NoPen);
    tag->geop->setOpacity(0);
    //add text label and hide until we see if user has enabled showLabel
    {
        QPen pen = QPen(Qt::blue);
        tag->tagLabel = new QGraphicsSimpleTextItem(NULL);
        tag->tagLabel->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        tag->tagLabel->setZValue(1);
        tag->tagLabel->setText(taglabel);
        tag->tagLabel->setOpacity(0);
        QFont font = tag->tagLabel->font();
        font.setPointSize(FONT_SIZE);
        font.setWeight(QFont::Normal);
        tag->tagLabel->setFont(font);
        pen.setStyle(Qt::SolidLine);
        pen.setWidthF(PEN_WIDTH);
        tag->tagLabel->setPen(pen);
        this->_scene->addItem(tag->tagLabel);
    }
}

/**
 * @fn    tagPos
 * @brief  update tag position on the screen (add to scene if it does not exist)
 *
 * */
void GraphicsWidget::tagPos(quint64 tagId, double x, double y, double z)
{
    //qDebug() << "tagPos Tag: 0x" + QString::number(tagId, 16) << " " << x << " " << y << " " << z;

    if(_busy || _geoFencingMode) //don't display position if geofencing is on
    {
        qDebug() << "(Widget - busy IGNORE) Tag: 0x" + QString::number(tagId, 16) << " " << x << " " << y << " " << z;
    }
    else
    {
        Tag *tag = NULL;
        bool newTag = false;
        QString t ;

        _busy = true ;
        tagIDToString(tagId, &t); //convert uint64 to string

        tag = _tags.value(tagId, NULL);

        if(!tag) //add new tag to the tags array
        {
            //tag does not exist, so create a new one
            newTag = true;
            addNewTag(tagId);
            tag = this->_tags.value(tagId, NULL);
        }

        if(!tag->p[tag->idx]) //we have not added this object yet to the history array
        {
            QAbstractGraphicsShapeItem *tag_pt = this->_scene->addEllipse(-1*_tagSize/2, -1*_tagSize/2, _tagSize, _tagSize);
            tag->p[tag->idx] = tag_pt;
            tag_pt->setPen(Qt::NoPen);

            if(tag->idx > 0) //use same brush settings for existing tag ID
            {
                tag_pt->setBrush(tag->p[0]->brush());
            }
            else //new brush... new tag ID as idx = 0
            {
                QBrush b = QBrush(QColor::fromHsvF(tag->colourH, tag->colourS, tag->colourV));
                QPen pen = QPen(b.color().darker());
                pen.setStyle(Qt::SolidLine);
                pen.setWidthF(PEN_WIDTH);
                //set the brush colour (average point is darker of the same colour
                tag_pt->setBrush(b.color().dark());
                tag->avgp->setBrush(b.color().darker());
                tag->tagLabel->setBrush(b.color().dark());
                tag->tagLabel->setPen(pen);
            }

            tag_pt->setToolTip(t);

            //update colour for next tag
            if(newTag) //keep same colour for same tag ID
            {
                tag->ridx = findTagRowIndex(t);

                if(tag->ridx == -1)
                {
                    tag->ridx = ui->tagTable->rowCount();

                    insertTag(tag->ridx, t, tag->r95Show, tag->showLabel, tag->tagLabelStr);
                }
            }
        }

        _ignore = true;
        ui->tagTable->item(tag->ridx,ColumnX)->setText(QString::number(x, 'f', 3));
        ui->tagTable->item(tag->ridx,ColumnY)->setText(QString::number(y, 'f', 3));
        ui->tagTable->item(tag->ridx,ColumnZ)->setText(QString::number(z, 'f', 3));

        tag->p[tag->idx]->setPos(x, y);

        if(_showHistory)
        {
            tagHistory(tagId);
            tag->idx = (tag->idx+1)%_historyLength;
        }
        else
        {
            //index will stay at 0
            tag->p[tag->idx]->setOpacity(1);
        }

        tag->tagLabel->setPos(x + 0.15, y + 0.15);
        _ignore = false;
        _busy = false ;

        //qDebug() << "Tag: 0x" + QString::number(tagId, 16) << " " << x << " " << y << " " << z;
	}
}


void GraphicsWidget::tagStats(quint64 tagId, double x, double y, double z, double r95)
{
    if(_busy)
    {
        qDebug() << "(busy IGNORE) R95: 0x" + QString::number(tagId, 16) << " " << x << " " << y << " " << z << " " << r95;
    }
    else
    {
        Tag *tag = NULL;

        _busy = true ;

        tag = _tags.value(tagId, NULL);

        if(!tag) //add new tag to the tags array
        {
            addNewTag(tagId);
            tag = this->_tags.value(tagId, NULL);
        }

        if (tag)
        {
            //static float h = 0.1;
            //float s = 0.5, v = 0.98;
            double rad = r95*2;

            if(tag->r95p)
            {
                //re-size the elipse... with a new rad value...
                tag->r95p->setOpacity(0); //hide it

                this->_scene->removeItem(tag->r95p);
                delete(tag->r95p);
                tag->r95p = NULL;
            }

            //else //add r95 circle

            {
                //add R95 circle
                tag->r95p = this->_scene->addEllipse(-1*r95, -1*r95, rad, rad);
                tag->r95p->setPen(Qt::NoPen);
                tag->r95p->setBrush(Qt::NoBrush);

                if( tag->r95Show && (rad <= 1))
                {
                    QPen pen = QPen(Qt::darkGreen);
                    pen.setStyle(Qt::DashDotDotLine);
                    pen.setWidthF(PEN_WIDTH);

                    tag->r95p->setOpacity(0.5);
                    tag->r95p->setPen(pen);
                    //tag->r95p->setBrush(QBrush(Qt::green, Qt::Dense7Pattern));
                    tag->r95p->setBrush(Qt::NoBrush);
                }
                else if (tag->r95Show && ((rad > 1)/*&&(rad <= 2)*/))
                {
                    QPen pen = QPen(Qt::darkRed);
                    pen.setStyle(Qt::DashDotDotLine);
                    pen.setWidthF(PEN_WIDTH);

                    tag->r95p->setOpacity(0.5);
                    tag->r95p->setPen(pen);
                    //tag->r95p->setBrush(QBrush(Qt::darkRed, Qt::Dense7Pattern));
                    tag->r95p->setBrush(Qt::NoBrush);
                }

            }

            //update Tag R95 value in the table
            {
                QString t ;
                int ridx = 0;

                tagIDToString(tagId, &t);

                ridx = findTagRowIndex(t);

                if(ridx != -1)
                {
                    ui->tagTable->item(ridx,ColumnR95)->setText(QString::number(r95, 'f', 3));
                }
                else
                {

                }
            }


            if(!tag->avgp) //add the average point
            {
                QBrush b = tag->p[0]->brush().color().darker();

                tag->avgp = this->_scene->addEllipse(-0.025, -0.025, 0.05, 0.05);
                tag->avgp->setBrush(b);
                tag->avgp->setPen(Qt::NoPen);
            }

            //if  (rad > 2)
            if(!tag->r95Show)
            {
                 tag->avgp->setOpacity(0);
            }
            else
            {
                tag->avgp->setOpacity(1);
                tag->avgp->setPos(x, y); //move it to the avg x and y values
            }

            tag->r95p->setPos(x, y); //move r95 center to the avg x and y values
        }
        else
        {
            //ERROR - there has to be a tag already...
            //ignore this statistics report
        }

        _busy = false ;

        qDebug() << "R95: 0x" + QString::number(tagId, 16) << " " << x << " " << y << " " << z << " " << r95;

    }
}


void GraphicsWidget::tagRange(quint64 tagId, quint64 aId, double range)
{
    if(_busy)
    {
        qDebug() << "(busy IGNORE) Range 0x" + QString::number(tagId, 16) << " " << range << " " << QString::number(aId, 16) ;
    }
    else
    {
        Tag *tag = NULL;

        _busy = true ;

        tag = _tags.value(tagId, NULL);

        if(!tag) //add new tag to the tags array
        {
            addNewTag(tagId);
            tag = this->_tags.value(tagId, NULL);
        }

        if (tag)
        {
            //Geo-fencing mode
            if(_geoFencingMode && (aId == 0) && (range >= 0))
            {
                QPen pen = QPen(Qt::darkGreen);

                if(tag->geop)
                {
                    //re-size the elipse... with a new rad value...
                    tag->geop->setOpacity(0); //hide it

                    this->_scene->removeItem(tag->geop);
                    delete(tag->geop);
                    tag->geop = NULL;
                }

                //if(!tag->geop) //add the circle around the anchor 0 which represents the tag
                {
                    pen.setColor(Qt::darkGreen);
                    pen.setStyle(Qt::DashDotDotLine);
                    pen.setWidthF(PEN_WIDTH);

                    if(_alarmOut) //then if > than outside zone
                    {
                        if(range >= _maxRad) //if > zone out then colour of zone out
                        {
                            pen.setColor(Qt::red);
                        }
                        else
                        {
                            if(range > _minRad) //if < zone out and > zone in
                            {
                                //amber
                                //pen.setColor(QColor::fromRgbF(1.0, 0.5, 0.0));
                                //blue
                                pen.setColor(QColor::fromRgbF(0.0, 0.0, 1.0));
                            }
                        }
                    }
                    else //then if < inside zone
                    {
                        if(range <= _minRad)
                        {
                            pen.setColor(Qt::red);
                        }
                        else
                        {
                            if(range < _maxRad) //if < zone out and > zone in
                            {
                                //amber
                                //pen.setColor(QColor::fromRgbF(1.0, 0.5, 0.0));
                                //blue
                                pen.setColor(QColor::fromRgbF(0.0, 0.0, 1.0));
                            }
                        }
                    }

                    tag->geop = this->_scene->addEllipse(-range, -range, range*2, range*2);
                    tag->geop->setPen(pen);
                    tag->geop->setBrush(Qt::NoBrush);
                }

                tag->geop->setOpacity(1);
                tag->geop->setPos(0, 0);

                tag->tagLabel->setPos(range + ((tagId & 0xF)*0.05 + 0.05), ((tagId & 0xF)*0.1 + 0.05));
            }

            //update Tag/Anchor range value in the table
            {
                QString t ;
                int ridx = 0;
                int anc = aId&0x3;

                tagIDToString(tagId, &t);

                ridx = findTagRowIndex(t);

                if(ridx != -1)
                {
                    if(range >= 0)
                    {
                        ui->tagTable->item(ridx,ColumnRA0+anc)->setText(QString::number(range, 'f', 3));
                    }
                    else
                    {
                        ui->tagTable->item(ridx,ColumnRA0+anc)->setText(" - ");
                    }
                }
                else //add tag into tag table
                {
                    tag->ridx = ui->tagTable->rowCount();

                    insertTag(tag->ridx, t, false, false, tag->tagLabelStr);
                }
            }
        }
        else
        {
            //if in tracking/navigation mode - ignore the range report
        }

    }
    _busy = false ;

}


void GraphicsWidget::tagHistory(quint64 tagId)
{
    int i = 0;
    int j = 0;

    Tag *tag = this->_tags.value(tagId, NULL);
    for(i = 0; i < _historyLength; i++)
    {
        QAbstractGraphicsShapeItem *tag_p = tag->p[i];

        if(!tag_p)
        {
            break;
        }
        else
        {
            j = (tag->idx - i); //element at index is opaque
            if(j<0) j+= _historyLength;
            tag_p->setOpacity(1-(float)j/_historyLength);
        }
    }
}

/**
 * @fn    tagHistoryNumber
 * @brief  set tag history length
 *
 * */
void GraphicsWidget::tagHistoryNumber(int value)
{
    bool tag_showHistory = _showHistory;

    while(_busy);

    _busy = true;

    //remove old history
    setShowTagHistory(false);

    //need to resize all the tag point arrays
    for (int i = 0; i < ui->tagTable->rowCount(); i++)
    {
        QTableWidgetItem* item = ui->tagTable->item(i, ColumnIDr);

        if(item)
        {
            bool ok;
            quint64 tagID = item->text().toULongLong(&ok, 16);
            //clear scene from any tags
            Tag *tag = _tags.value(tagID, NULL);
            tag->p.resize(value);
        }
    }

    //set new history length
    _historyLength = value;

    //set the history to show/hide
    _showHistory = tag_showHistory;

    _busy = false;
}


/**
 * @fn    zone1Value
 * @brief  set Zone 1 radius
 *
 * */
void GraphicsWidget::zone1Value(double value)
{
    if(zone1)
    {
        this->_scene->removeItem(zone1);
        delete(zone1);
        zone1 = NULL;
    }

    zone(0, value, _zone1Red);
}

/**
 * @fn    zone2Value
 * @brief  set Zone 2 radius
 *
 * */
void GraphicsWidget::zone2Value(double value)
{
    if(zone2)
    {
        this->_scene->removeItem(zone2);
        delete(zone2);
        zone2 = NULL;
    }

    zone(1, value, _zone2Red);
}


/**
 * @fn    setAlarm
 * @brief  set alarms
 *
 * */
void GraphicsWidget::setAlarm(bool in, bool out)
{
    Q_UNUSED(in);
    if(zone1)
    {
        zone1->setOpacity(0);
        this->_scene->removeItem(zone1);
        delete(zone1);
        zone1 = NULL;
    }

    if(zone2)
    {
        zone2->setOpacity(0);
        this->_scene->removeItem(zone2);
        delete(zone2);
        zone2 = NULL;
    }

    if(out)
    {
        _alarmOut = true;

        //set outside zone to red
        if(_zone1Rad > _zone2Rad)
        {
            zone(0, _zone1Rad, true);

            zone(1, _zone2Rad, false);
        }
        else
        {
            zone(0, _zone1Rad, false);

            zone(1, _zone2Rad, true);
        }
    }
    else
    {
        _alarmOut = false;

        //set inside zone to red
        if(_zone1Rad < _zone2Rad)
        {
            zone(0, _zone1Rad, true);

            zone(1, _zone2Rad, false);
        }
        else
        {
            zone(0, _zone1Rad, false);

            zone(1, _zone2Rad, true);
        }
    }
}

/**
 * @fn    zones
 * @brief  set Zone radius
 *
 * */
void GraphicsWidget::zone(int zone, double radius, bool red)
{
    //add Zone 1
    if(zone == 0)
    {
        QPen pen = QPen(QBrush(Qt::green), 0.005) ;

        _zone1Red = false;

        if(red)
        {
            pen.setColor(Qt::red);
            _zone1Red = true;
        }
        pen.setStyle(Qt::SolidLine);
        pen.setWidthF(0.03);

        if(!zone1)
        {
            zone1 = this->_scene->addEllipse(-_zone1Rad, -_zone1Rad, 2*_zone1Rad, 2*_zone1Rad);
        }
        zone1->setPen(pen);
        zone1->setBrush(Qt::NoBrush);
        zone1->setToolTip("Zone1");
        zone1->setPos(0, 0);
        _zone1Rad = radius;

        if(_geoFencingMode)
        {
            zone1->setOpacity(1);
        }
        else
        {
            zone1->setOpacity(0);
        }
    }
    else
    //add Zone 2
    {
        QPen pen = QPen(QBrush(Qt::green), 0.005) ;

        _zone2Red = false;

        if(red)
        {
            pen.setColor(Qt::red);
            _zone2Red = true;
        }
        pen.setStyle(Qt::SolidLine);
        pen.setWidthF(0.03);

        if(!zone2)
        {
            zone2 = this->_scene->addEllipse(-_zone2Rad, -_zone2Rad, 2*_zone2Rad, 2*_zone2Rad);
        }
        zone2->setPen(pen);
        zone2->setBrush(Qt::NoBrush);
        zone2->setToolTip("Zone2");
        zone2->setPos(0, 0);
        _zone2Rad = radius;

        if(_geoFencingMode)
        {
            zone2->setOpacity(1);
        }
        else
        {
            zone2->setOpacity(0);
        }
    }

    _maxRad = (_zone1Rad > _zone2Rad) ? _zone1Rad : _zone2Rad;
    _minRad = (_zone1Rad > _zone2Rad) ? _zone2Rad : _zone1Rad;
}

void GraphicsWidget::centerOnAnchors(void)
{

    Anchor *a1 = this->_anchors.value(0, NULL);
    Anchor *a2 = this->_anchors.value(1, NULL);
    Anchor *a3 = this->_anchors.value(2, NULL);
    //Anchor *a4 = anc = this->anchors.value(0, NULL);

    QPolygonF p1 = QPolygonF() << QPointF(a1->a->pos()) << QPointF(a2->a->pos()) << QPointF(a3->a->pos()) ;


    emit centerRect(p1.boundingRect());

}

/**
 * @fn    showGeoFencingMode
 * @brief  if Geo-fencing mode selected - then place anchor 0 in the middle
 *         and draw two concentric circles for the two zones
 *         hide other anchors
 *
 * */
void GraphicsWidget::showGeoFencingMode(bool set)
{
    Anchor *anc;
    int i;

    if(set)
    {
        double a1x = 0.0;
        double a1y = 0.0;
        double a1z = 0.0;

        bool showHistoryTemp = _showHistory;

        //disable tag history and clear any
        setShowTagHistory(false);

        _showHistoryP = showHistoryTemp ; //so that the show/hide tag history will be restored when going back to tracking mode

        //clear all Tag entries
        clearTags();

        //place anchor ID 0 at 0,0
        anchPos(0, a1x, a1y, a1z, true, false);

        zone(0, _zone1Rad, _zone1Red);

        zone(1, _zone2Rad, _zone2Red);

        zone1->setOpacity(1);
        zone2->setOpacity(1);

        for(i=1; i<4; i++)
        {
            anc = this->_anchors.value(i, NULL);

            if(anc)
            {
                anc->a->setOpacity(0);
                anc->ancLabel->setOpacity(0);
            }
        }

        if(0)
        {
            //center the scene on anchor 0
            emit centerAt(a1x, a1y);
        }
        else //zoom out so the zones can be seen
        {

            double rad = _zone2Rad > _zone1Rad ? _zone2Rad : _zone1Rad ;
            QPolygonF p1 = QPolygonF() << QPointF( a1x - rad, a1y) << QPointF(a1x + rad, a1y) << QPointF(a1x, a1y + rad) << QPointF(a1x, a1y - rad) ;

            emit centerRect(p1.boundingRect());
        }


        _geoFencingMode = true;
    }
    else
    {

        //clear all Tag entries
        clearTags();

        for(i=0; i<4; i++)
        {
            anc = this->_anchors.value(i, NULL);

            if(anc)
            {
                if(anc->show)
                {
                    anc->a->setOpacity(1);
                    anc->ancLabel->setOpacity(1);
                }
            }
        }

        zone1->setOpacity(0);
        zone2->setOpacity(0);

        this->_scene->removeItem(zone1);
        delete(zone1);
        zone1 = NULL;

        this->_scene->removeItem(zone2);
        delete(zone2);
        zone2 = NULL;

        setShowTagHistory(_showHistoryP);

        //center the centriod of the triangle given by anchors (0,1,2) points
        //

        centerOnAnchors();

        _geoFencingMode = false;
    }

    ui->tagTable->setColumnHidden(ColumnX,_geoFencingMode); //x
    ui->tagTable->setColumnHidden(ColumnY,_geoFencingMode); //y
    ui->tagTable->setColumnHidden(ColumnZ,_geoFencingMode); //z
    ui->tagTable->setColumnHidden(ColumnR95,_geoFencingMode); //r95
    ui->tagTable->setColumnHidden(ColumnRA1,_geoFencingMode); //range to A1
    ui->tagTable->setColumnHidden(ColumnRA2,_geoFencingMode); //range to A2
    ui->tagTable->setColumnHidden(ColumnRA3,_geoFencingMode); //range to A3
}

/**
 * @fn    setShowTagAncTable
 * @brief  hide/show Tag and Anchor tables
 *
 * */
void GraphicsWidget::setShowTagAncTable(bool anchorTable, bool tagTable, bool ancTagCorr)
{

    if(!tagTable)
    {
        ui->tagTable->hide();
    }
    else
    {
        ui->tagTable->show();
    }

    //hide or show Anchor-Tag correction table
    hideTACorrectionTable(!ancTagCorr);

    if(!anchorTable)
    {
        ui->anchorTable->hide();
    }
    else
    {
        ui->anchorTable->show();
    }

}

/**
 * @fn    setShowTagHistory
 * @brief  if show Tag history is checked then display last N Tag locations
 *         else only show the current/last one
 *
 * */
void GraphicsWidget::setShowTagHistory(bool set)
{
    _busy = true ;

    if(set != _showHistory) //the value has changed
    {
        //for each tag
        if(set == false) //we want to hide history - clear the array
        {
            QMap<quint64, Tag*>::iterator i = _tags.begin();

            while(i != _tags.end())
            {
                Tag *tag = i.value();
                for(int idx=0; idx<_historyLength; idx++ )
                {
                    QAbstractGraphicsShapeItem *tag_p = tag->p[idx];
                    if(tag_p)
                    {
                        tag_p->setOpacity(0); //hide it

                        this->_scene->removeItem(tag_p);
                        delete(tag_p);
                        tag_p = NULL;
                        tag->p[idx] = 0;
                    }
                }
                tag->idx = 0; //reset history
                i++;
            }
        }
        else
        {

        }

        _showHistoryP = _showHistory = set; //update the value
    }

    _busy = false;
}

/**
 * @fn    addNewAnchor
 * @brief  add new Anchor with anchId into the tags QMap
 *
 * */
void GraphicsWidget::addNewAnchor(quint64 anchId, bool show)
{
    Anchor *anc;

    qDebug() << "Add new Anchor: 0x" + QString::number(anchId, 16);

    //insert into table, and create an array to hold history of its positions
    _anchors.insert(anchId,new(Anchor));
    anc = this->_anchors.value(anchId, NULL);

    anc->a = NULL;
    //add text label and show by default
    {
        anc->ancLabel = new QGraphicsSimpleTextItem(NULL);
        anc->ancLabel->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        anc->ancLabel->setZValue(1);
        anc->ancLabel->setText(QString::number(anchId));
        QFont font = anc->ancLabel->font();
        font.setPointSize(FONT_SIZE);
        font.setWeight(QFont::Normal);
        anc->ancLabel->setFont(font);
        this->_scene->addItem(anc->ancLabel);
    }
    anc->show = show;

}

/**
 * @fn    ancRanges
 * @brief  the inputs are anchor to anchor ranges
 *         e.g. 0,1 is anchor 0 to anchor 1
 *         e.g. 0,2 is anchor 0 to anchor 2
 * */
void GraphicsWidget::ancRanges(int a01, int a02, int a12)
{
    if(_busy)
    {
        qDebug() << "(Widget - busy IGNORE) anchor ranges";
    }
    else
    {
        Anchor *anc0 = _anchors.value(0x0, NULL);
        Anchor *anc1 = _anchors.value(0x1, NULL);
        Anchor *anc2 = _anchors.value(0x2, NULL);

        qreal rA01 = ((double)a01)/1000;
        qreal rA02 = ((double)a02)/1000;
        qreal rA12 = ((double)a12)/1000;

        _busy = true ;

        qDebug() << "Anchor Ranges: " << rA01 << rA02 << rA12 ;

        if((anc0 != NULL) && (anc1 != NULL) && (anc2 != NULL))
        {
            QPen pen = QPen(QColor::fromRgb(85, 60, 150, 255));
            qreal x_coord = anc0->p.x() + rA01;
            qreal pwidth = 0.025;
            //qreal penw = pen.widthF() ;
            pen.setWidthF(pwidth);
            //penw = pen.widthF() ;
            QLineF line01 = QLineF(anc0->p, QPointF(x_coord, anc0->p.y()));


            if(!_line01)
            {
                _line01 = this->_scene->addLine(line01, pen);
            }
            else
            {
                _line01->setLine(line01);
            }
        }
        _busy = false ;
    }
}

/**
 * @fn    anchPos
 * @brief  add an anchor to the database and show on screen
 *
 * */
void GraphicsWidget::anchPos(quint64 anchId, double x, double y, double z, bool show, bool update)
{
    if(_busy)
    {
        qDebug() << "(Widget - busy IGNORE) Anch: 0x" + QString::number(anchId, 16) << " " << x << " " << y << " " << z;
    }
    else
    {
        Anchor *anc = NULL;

        _busy = true ;

        anc = _anchors.value(anchId, NULL);

        if(!anc) //add new anchor to the anchors array
        {
            addNewAnchor(anchId, show);
            insertAnchor(anchId, x, y, z, RTLSDisplayApplication::instance()->client()->getTagCorrections(anchId), show);
            anc = this->_anchors.value(anchId, NULL);
        }

        //add the graphic shape (anchor image)
        if (anc->a == NULL)
        {
            QAbstractGraphicsShapeItem *anch = this->_scene->addEllipse(-ANC_SIZE/2, -ANC_SIZE/2, ANC_SIZE, ANC_SIZE);
            anch->setPen(Qt::NoPen);
            anch->setBrush(QBrush(QColor::fromRgb(85, 60, 150, 255)));
            anch->setToolTip("0x"+QString::number(anchId, 16));
            anc->a = anch;
        }

        anc->a->setOpacity(anc->show ? 1.0 : 0.0);
        anc->ancLabel->setOpacity(anc->show ? 1.0 : 0.0);

        anc->ancLabel->setPos(x + 0.15, y + 0.15);
        anc->a->setPos(x, y);

        if(update) //update Table entry
        {
            int r = anchId & 0x3;
            _ignore = true;
            ui->anchorTable->item(r,ColumnX)->setText(QString::number(x, 'f', 2));
            ui->anchorTable->item(r,ColumnY)->setText(QString::number(y, 'f', 2));
            _ignore = false;
        }
        _busy = false ;
        //qDebug() << "(Widget) Anch: 0x" + QString::number(anchId, 16) << " " << x << " " << y << " " << z;
    }
}

/**
 * @fn    anchTableEditing
 * @brief  enable or disable editing of anchor table
 *
 * */
void GraphicsWidget::anchTableEditing(bool enable)
{
    //ui->anchorTable->setEnabled(enable);

    if(!enable)
    {
        for(int i = 0; i<3; i++)
        {
            Qt::ItemFlags flags = ui->anchorTable->item(i,ColumnX)->flags();
            flags = flags &  ~(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled) ;
            ui->anchorTable->item(i,ColumnX)->setFlags(flags);
            ui->anchorTable->item(i,ColumnY)->setFlags(flags);
        }
    }
    else
    {
        for(int i = 0; i<3; i++)
        {
            Qt::ItemFlags flags = ui->anchorTable->item(i,ColumnX)->flags();
            ui->anchorTable->item(i,ColumnX)->setFlags(flags | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
            ui->anchorTable->item(i,ColumnY)->setFlags(flags | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        }
    }

}
