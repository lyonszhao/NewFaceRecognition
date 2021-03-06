/** ===========================================================
 * @file
 *
 * This file is a part of digiKam project
 * <a href="http://www.digikam.org">http://www.digikam.org</a>
 *
 * @date   2010-06-21
 * @brief  GUI test program for libkface
 *
 * @author Copyright (C) 2010 by Alex Jironkin
 *         <a href="mailto:alexjironkin at gmail dot com">alexjironkin at gmail dot com</a>
 *         Copyright (C) 2010 by Aditya Bhatt
 *         <a href="mailto:adityabhatt1991 at gmail dot com">adityabhatt1991 at gmail dot com</a>
 *         Copyright (C) 2010-2013 by Gilles Caulier
 *         <a href="mailto:caulier dot gilles at gmail dot com">caulier dot gilles at gmail dot com</a>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include "mainwindow.moc"
#include "ui_mainwindow.h"

// C++ includes

#include <cstdlib>
#include <iostream>

// Qt includes

#include <QLayout>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QFileDialog>
#include <QLabel>

// KDE include

#include <kdebug.h>
#include <kfiledialog.h>

// libkface includes

#include "../libkface/database.h"
#include "../libkface/face.h"
#include "../libkface/kfaceutils.h"

// Local includes

#include "faceitem.h"

using namespace tld;
using namespace std;
using namespace KFaceIface;

class MainWindow::Private
{
public:

    Private()
    {
        ui            = new Ui::MainWindow;
        myScene       = 0;
        myView        = 0;
        lastPhotoItem = 0;
        database      = 0;
        scale         = 0.0;
    }

    Ui::MainWindow*      ui;
    QGraphicsScene*      myScene;
    QGraphicsView*       myView;
    QGraphicsPixmapItem* lastPhotoItem;
    QList<FaceItem*>     faceitems;
    Database*            database;
    QImage               currentPhoto;
    double               scale;
    QList<Face>          currentFaces;
    QString              lastFileOpenPath;
};

MainWindow::MainWindow(QWidget* const parent)
    : QMainWindow(parent), d(new Private)
{
    d->ui->setupUi(this);

    connect(d->ui->openImageBtn, SIGNAL(clicked()),
            this, SLOT(openImage()));

    connect(d->ui->openConfigBtn, SIGNAL(clicked()),
            this, SLOT(openConfig()));

    connect(d->ui->detectFacesBtn, SIGNAL(clicked()),
            this, SLOT(detectFaces()));

    connect(d->ui->recogniseBtn, SIGNAL(clicked()),
            this, SLOT(recognise()));

    connect(d->ui->updateDatabaseBtn, SIGNAL(clicked()),
            this, SLOT(updateConfig()));

    connect(d->ui->horizontalSlider, SIGNAL(valueChanged(int)),
            this, SLOT(updateAccuracy()));

    d->myScene                = new QGraphicsScene();
    QGridLayout* const layout = new QGridLayout;
    d->myView                 = new QGraphicsView(d->myScene);

    d->myView->setCacheMode(QGraphicsView::CacheBackground);
    d->myScene->setItemIndexMethod(QGraphicsScene::NoIndex);

    setMouseTracking(true);
    layout->addWidget(d->myView);

    d->ui->widget->setLayout(layout);

    d->myView->show();

    d->database         = new Database(Database::InitAll, ".");

    d->ui->configLocation->setText(QDir::currentPath());
    d->ui->horizontalSlider->setValue(d->database->detectionAccuracy());

    d->lastFileOpenPath = QDir::currentPath();
}

MainWindow::~MainWindow()
{
    delete d->ui;
    delete d;
}

void MainWindow::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);

    switch (e->type())
    {
        case QEvent::LanguageChange:
            d->ui->retranslateUi(this);
            break;
        default:
            break;
    }
}

void MainWindow::openImage()
{
    QString file = KFileDialog::getOpenFileName(d->lastFileOpenPath,
                                                "Image Files (*.png *.jpg *.bmp *.pgm)",
                                                this,
                                                "Open Image");

    if (file.isEmpty())
        return;

    d->lastFileOpenPath = QFileInfo(file).absolutePath();

    clearScene();

    kDebug() << "Opened file " << file.toAscii().data();

    QPixmap* const photo = new QPixmap(file);
    d->lastPhotoItem     = new QGraphicsPixmapItem(*photo);
    d->currentPhoto      = photo->toImage();

    if(1.0*d->ui->widget->width()/photo->width() < 1.*d->ui->widget->height()/photo->height())
    {
        d->scale = 1.0*d->ui->widget->width()/photo->width();
    }
    else
    {
        d->scale = 1.0*d->ui->widget->height()/photo->height();
    }

    d->lastPhotoItem->setScale(d->scale);

    d->myScene->addItem(d->lastPhotoItem);
}

void MainWindow::openConfig()
{
    QString directory = KFileDialog::getExistingDirectory(QDir::currentPath(),
                                                          this,
                                                          "Select Config Directory");

    d->ui->configLocation->setText(directory);

    d->database = new Database(Database::InitAll, directory);
}

void MainWindow::detectFaces()
{
    d->currentFaces.clear();

    d->currentFaces = d->database->detectFaces(d->currentPhoto);
    Face face;
    kDebug() << "libkface detected : " << d->currentFaces.size() << " faces.";

    foreach(FaceItem* const item, d->faceitems)
        item->setVisible(false);

    d->faceitems.clear();

    for(int i = 0; i < d->currentFaces.size(); ++i)
    {
        face = d->currentFaces[i];
        d->faceitems.append(new FaceItem(0, d->myScene, face.toRect(), d->scale));
        kDebug() << face.toRect()<<endl;
    }
}

void MainWindow::updateConfig()
{
    kDebug() << "Path of config directory = " << d->database->configPath();

    // Assign the text of the d->faceitems to the name of each face. When there is no text, drop that face from currentfaces.
    QList<Face> updateList;

    for(int i = 0 ; i <d->currentFaces.size(); ++i)
    {
        if(d->faceitems[i]->text() != "?")
        {
            d->currentFaces[i].setName(d->faceitems[i]->text());
            updateList.append(d->currentFaces.at(i));
        }
    }

    if( d->database->updateFaces(updateList,d->currentPhoto) )
    {
        kDebug() << "Trained";
        d->database->saveConfig();
    }
    else
    {
        kDebug() << "No faces to train.";
    }
}

void MainWindow::updateAccuracy()
{
    int value = d->ui->horizontalSlider->value();
    d->ui->lcdNumber->display(value);
    d->database->setDetectionAccuracy(value);
}

void MainWindow::clearScene()
{
    QList<QGraphicsItem*> list = d->myScene->items();

    for(int i=0; i<list.size(); i++)
    {
        d->myScene->removeItem(list.at(i));
    }
}

void MainWindow::recognise()
{
    if(!d->database->recognizeFaces(d->currentFaces,d->currentPhoto))
        return;

    for(int i = 0; i < d->currentFaces.size(); ++i)
    {
        d->faceitems[i]->suggest(d->currentFaces[i].name());
        kDebug() << "Face #"<< i+1 << " is closest to the person with ID " << d->currentFaces[i].id()
                 << " and name "<< d->currentFaces[i].name();
    }
}
