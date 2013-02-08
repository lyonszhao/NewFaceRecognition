/** ===========================================================
 * @file
 *
 * This file is a part of digiKam project
 * <a href="http://www.digikam.org">http://www.digikam.org</a>
 *
 * @date   2010-06-16
 * @brief  The Database class wraps the libface database
 *
 * @author Copyright (C) 2010-2013 by Marcel Wiesweg
 *         <a href="mailto:marcel dot wiesweg at gmx dot de">marcel dot wiesweg at gmx dot de</a>
 * @author Copyright (C) 2010 by Aditya Bhatt
 *         <a href="mailto:adityabhatt1991 at gmail dot com">adityabhatt1991 at gmail dot com</a>
 * @author Copyright (C) 2010-2013 by Gilles Caulier
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

#include "database.h"

// Qt includes

#include <QMutex>
#include <QMutexLocker>
#include <QSharedData>
#include <QFile>

// KDE includes

#include <kdebug.h>
#include <kstandarddirs.h>

// Libface includes

#include "../libface/LibFace.h"
#include "../libface/Face.h"

// Local includes

#include "kfaceutils.h"
#include "image_p.h"

using namespace std;

namespace KFaceIface
{

class Database::Private : public QSharedData
{
public:

    Private()
        : mappingFilename(QString("/dictionary")),
          haarCascasdePath(KStandardDirs::installPath("data") + QString("libkface/haarcascades"))
    {
        libface          = 0;
        configDirty      = false;
    }

    ~Private()
    {
        saveConfig();

        try
        {
            delete libface;
        }
        catch (cv::Exception& e)
        {
            kError() << "cv::Exception:" << e.what();
        }
        catch(...)
        {
            kDebug() << "cv::Exception";
        }
    }

    void saveConfig()
    {
        try
        {
            if ((initFlags & Database::InitRecognition) && configDirty)
            {
                libface->saveConfig(configPath.toStdString());
                configDirty = false;
            }
        }
        catch (cv::Exception& e)
        {
            kError() << "cv::Exception:" << e.what();
        }
        catch(...)
        {
            kDebug() << "cv::Exception";
        }
    }

public:

    libface::LibFace*   libface;
    Database::InitFlags initFlags;
    QHash<QString, int> hash;
    QString             configPath;
    bool                configDirty;
    const QString       mappingFilename;
    const QString       haarCascasdePath;
};

Database::Database(InitFlags flags, const QString& configurationPath)
    : d(new Private)
{
    // Note: same lines in RecognitionDatabase. Keep in sync.
    if (configurationPath.isNull())
        d->configPath = KStandardDirs::locateLocal("data", "libkface/database/", true);
    else
        d->configPath = configurationPath;

    d->hash      = KFaceUtils::hashFromFile(d->configPath + d->mappingFilename);
    d->initFlags = flags;

    try
    {
        if (flags == InitDetection)
        {
            d->libface = new libface::LibFace(libface::DETECT, d->configPath.toStdString(), d->haarCascasdePath.toStdString());
        }
        else
        {
            libface::Mode mode;

            if (flags == InitAll)
                mode = libface::ALL;
            else
                mode = libface::EIGEN;

            d->libface = new libface::LibFace(mode, d->configPath.toStdString(), d->haarCascasdePath.toStdString());
        }
    }
    catch (cv::Exception& e)
    {
        kError() << "cv::Exception:" << e.what();
    }
    catch(...)
    {
        kDebug() << "cv::Exception";
    }

    if (flags & InitDetection)
        this->setDetectionAccuracy(3);
}

Database::Database(const Database& other)
{
    d = other.d;
}

Database& Database::operator=(const KFaceIface::Database& other)
{
    d = other.d;
    return *this;
}

Database::~Database()
{
    // note: saveConfig is done in Priv destructor!
}

QList<Face> Database::detectFaces(const Image& image) const
{
    const IplImage* const img = image.imageData();
    //cvShowImage("show1",img );
    cvWaitKey(0);
    CvSize originalSize       = cvSize(0,0);

    if (!image.originalSize().isNull())
        originalSize = KFaceUtils::toCvSize(image.originalSize());

    std::vector<libface::Face> result;

    try
    {
        result = d->libface->detectFaces(img, originalSize);
    }
    catch (cv::Exception& e)
    {
        kError() << "cv::Exception:" << e.what();
    }
    catch(...)
    {
        kDebug() << "cv::Exception";
    }

    QList<Face>                          faceList;
    std::vector<libface::Face>::iterator it;

    for (it = result.begin(); it != result.end(); ++it)
    {
        faceList << Face::fromFace(*it, Face::ShallowCopy);
    }

    return faceList;
}

bool Database::updateFaces(QList<Face>& faces, const QImage& ImageToTld) const
{
    if(faces.isEmpty())
        return false;

    std::vector<libface::Face> faceVec;

    foreach(Face face, faces)
    {
        // If a name is already there in the dictionary, then set the ID from the dictionary, so that libface won't set it's own ID
        if(d->hash.contains(face.name()))
        {
            face.setId(d->hash[face.name()]);
            //cout << face.toFace().getX1() << endl;
        }
        else if(face.name() != NULL)
        {
            QImage faceToTld               = ImageToTld.copy(face.toFace().getX1(),face.toFace().getY1(),
                                                             face.toFace().getWidth(),face.toFace().getHeight());
            libface::Tldface* const tmpTLD = new libface::Tldface;
            tmpTLD->writeModelTofile((tmpTLD->QImage2IplImage(faceToTld)),(face.name().toStdString().c_str()));
            delete tmpTLD;
        }

        faceVec.push_back(face.toFace(Face::ShallowCopy));
    }

    std::vector<int> ids;

    try
    {
        ids = d->libface->update(&faceVec);
    }
    catch (cv::Exception& e)
    {
        kError() << "cv::Exception:" << e.what();
    }
    catch(...)
    {
        kDebug() << "cv::Exception";
    }

    for(int i = 0; i<(int)ids.size(); ++i)
    {
        faces[i].setId(ids.at(i));

        // If the name was not in the mapping before (new name), add it to the dictionary
        if(!d->hash.contains(faces[i].name()))
        {
            // Add to file
            KFaceUtils::addNameToFile(d->configPath + d->mappingFilename, faces[i].name(), faces[i].id());
            // Add to d->hash
            d->hash[faces[i].name()] = faces[i].id();
        }
    }

    d->configDirty = true;

    return true;
}

QList<double> Database::recognizeFaces(QList<Face>& faces, const QImage& imageToTld) const
{
    QList<double> closeness;

    if(faces.isEmpty() || !d->libface->count())
    {
        return closeness;
    }

    std::vector<libface::Face> faceVec;

    foreach(const Face& face, faces)
    {
        faceVec.push_back(face.toFace(Face::ShallowCopy));
    }

    std::vector< std::pair<int, double> > result;

    try
    {
        result = d->libface->recognise(&faceVec);
    }
    catch (cv::Exception& e)
    {
        kError() << "cv::Exception:" << e.what();
    }
    catch(...)
    {
        kDebug() << "cv::Exception";
    }

    for(int i = 0; i <faces.size() && i<(int)result.size(); ++i)
    {
        faces[i].setId(result.at(i).first);
        closeness.append(result.at(i).second);

/*
        // Locate the name from the hash, pity we don't have a bi-directional hash in Qt

        QHashIterator<QString, int> it(d->hash);
        it.toFront();

        while(it.hasNext())
        {
            it.next();
            cout << it.key().toStdString() << endl;

            if(it.value() == faces[i].id())
            {
                faces[i].setName(it.key());
                break;
            }
        }
*/
        vector< string> namesInDatabase;
        vector<float> recognitionConfidence;

        QHashIterator<QString, int> itp(d->hash);
        int count = -1;

        while(itp.hasNext())
        {
           itp.next();

           QImage faceToTld = imageToTld.copy(faces[i].toFace().getX1(),    faces[i].toFace().getY1(),
                                              faces[i].toFace().getWidth(), faces[i].toFace().getHeight());
           if(itp.key() != NULL)
           {
                count ++;
                libface::Tldface* const tmpTLD = new libface::Tldface;
                cout << "failed" << endl;
                recognitionConfidence.push_back((tmpTLD->getRecognitionConfidence((tmpTLD->QImage2IplImage(faceToTld)),(itp.key().toStdString().c_str()))));
                namesInDatabase.push_back(itp.key().toStdString());

                delete tmpTLD;
            }
        }

        if(count != -1)
        {
            int maxConfIndex    = 0;
            float maxConfidence = recognitionConfidence[0];

            for(int tmpInt = 0; tmpInt <= count ; tmpInt++ )
            {
                if(recognitionConfidence[tmpInt] > maxConfidence)
                {
                    maxConfIndex  = tmpInt;
                    maxConfidence = recognitionConfidence[tmpInt];
                }
            }

            cout << "executed " << endl;

            if(maxConfidence > 0.6 )
                faces[i].setName(QString::fromStdString(namesInDatabase[maxConfIndex]));
        }
    }

    return closeness;
}

void Database::clearTraining(const QString& name)
{
    Q_UNUSED(name)
    //TODO
}

void Database::clearTraining(int id)
{
    Q_UNUSED(id)
    //TODO
}

void Database::clearAllTraining()
{
    d->libface->loadConfig(std::map<std::string, std::string>());
}

void Database::saveConfig()
{
    d->saveConfig();
}

QString Database::configPath() const
{
    return d->configPath;
}

void Database::setDetectionAccuracy(double value)
{
    d->libface->setDetectionAccuracy(value);
}

double Database::detectionAccuracy() const
{
    return d->libface->getDetectionAccuracy();
}

void Database::setDetectionSpecificity(double value)
{
    d->libface->setDetectionSpecificity(value);
}

double Database::detectionSpecificity() const
{
    return d->libface->getDetectionSpecificity();
}

int Database::peopleCount() const
{
    return d->libface->count();
}

QString Database::nameForId(int id) const
{
    return d->hash.key(id);
}

int Database::idForName(const QString& name) const
{
    return d->hash.value(name, -1);
}

QList<int> Database::allIds() const
{
    return d->hash.values();
}

QStringList Database::allNames() const
{
    return d->hash.keys();
}

int Database::recommendedImageSizeForDetection(const QSize& size) const
{
    return d->libface->getRecommendedImageSizeForDetection(KFaceUtils::toCvSize(size));
}

QSize Database::recommendedImageSizeForRecognition(const QSize& size) const
{
    return KFaceUtils::fromCvSize(d->libface->getRecommendedImageSizeForRecognition(KFaceUtils::toCvSize(size)));
}

} // namespace KFaceIface
