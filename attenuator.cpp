#include "attenuator.h"
#include <QDir>
#include <math.h>

Attenuator::Attenuator(QObject *parent) :
	HardwareObject(parent)
{
    d_key = QString("attn");
}

Attenuator::~Attenuator()
{

}

void Attenuator::initialize()
{
    QSettings s(QSettings::SystemScope,QApplication::organizationName(),QApplication::applicationName());
    QString attenDataFile = s.value(QString("%1/file").arg(d_key),QString("")).toString();

    if(!parseAttenFile(attenDataFile))
	   emit logMessage(QString("No valid attenuation table. Tuning attenuation will not be set automatically!"),QtFTM::LogWarning);
}

void Attenuator::changeAttenFile(QString fileName)
{
    if(parseAttenFile(fileName))
    {
        QSettings s(QSettings::SystemScope,QApplication::organizationName(),QApplication::applicationName());
        s.setValue(QString("%1/file").arg(d_key),fileName);
        s.sync();
    }
}

int Attenuator::setTuningAttn(double freq)
{
    int atten = 0;
    blockSignals(true);

    if(d_attenData.isEmpty())
    {
        atten = readAttn();
        blockSignals(false);
        emit taattnUpdate(atten);
        return atten;
    }
    if(freq < d_attenData.at(0).first)
    {
        atten = setAttn(0);
        blockSignals(false);
        emit taattnUpdate(atten);
        return atten;
    }
    if(freq >= d_attenData.at(d_attenData.size()-1).first)
    {
        atten = setAttn(0);
        blockSignals(false);
        emit taattnUpdate(atten);
        return atten;
    }

    //interpolate (note: could change this to a binary search instead of a straight-through loop, but the performance gain would be insignificant)
    for(int i=0;i<d_attenData.size();i++)
    {
        if(fabs(d_attenData.at(i).first-freq) < 0.001)
        {
            atten = d_attenData.at(i).second;
            break;
        }

        if(freq > d_attenData.at(i).first && freq < d_attenData.at(i+1).first)
        {
            atten = (int)round(d_attenData.at(i).second + ((double)d_attenData.at(i+1).second - (double)d_attenData.at(i).second)/(d_attenData.at(i+1).first - d_attenData.at(i).first)*(freq-d_attenData.at(i).first));
            break;
        }
    }


    int actual_attn = setAttn(qMax(atten,0));
    blockSignals(false);

    emit taattnUpdate(actual_attn);
    return actual_attn;

}

void Attenuator::clearAttenData()
{
    d_attenData.clear();
}

bool Attenuator::parseAttenFile(QString fileName)
{
    QFile f(fileName);
    if(!f.exists())
    {
        emit logMessage(QString("The specified attenuation data file (%1) does not exist. The tuning attenuation table has not been changed.").arg(f.fileName()),QtFTM::LogWarning);
        emit attenFileParseSuccess(false);
        return false;
    }

    if(!f.open(QIODevice::ReadOnly))
    {
        emit logMessage(QString("The specified attenuation data file (%1) could not be opened. The tuning attenuation table has not been changed.").arg(f.fileName()),QtFTM::LogWarning);
        emit attenFileParseSuccess(false);
        return false;
    }

    QList<QPair<double,int> > newData;

    while(!f.atEnd())
    {
        QByteArray dat = f.readLine();
        if(dat.startsWith('#'))
            continue;

        QList<QByteArray> l = dat.split('\t');
        if(l.size() < 2)
        {
            emit logMessage(QString("Could not parse attentuation data entry: %1. Skipping...").arg(QString(dat)),QtFTM::LogWarning);
            continue;
        }

        bool ok = false;
        double f = l.at(0).trimmed().toDouble(&ok);
        if(!ok)
        {
            emit logMessage(QString("Could not parse attentuation data entry: %1. Skipping...").arg(QString(dat)),QtFTM::LogWarning);
            continue;
        }

        ok = false;
        int a = l.at(1).trimmed().toInt(&ok);
        if(!ok)
        {
            emit logMessage(QString("Could not parse attentuation data entry: %1. Skipping...").arg(QString(dat)),QtFTM::LogWarning);
            continue;
        }

        newData.append(QPair<double,int>(f,a));
    }

    f.close();

    if(newData.isEmpty())
    {
        emit logMessage(QString("Could not read attenuation data file %1. The tuning attenuation table has not been changed.").arg(f.fileName()),QtFTM::LogWarning);
        emit attenFileParseSuccess(false);
        return false;
    }
    else
    {
        emit logMessage(QString("Successfully read tuning attenuation table from %1 (%2 entries).").arg(f.fileName()).arg(newData.size()));
        emit logMessage(QString("Attenuation table covers the range %1 - %2 MHz.").arg(newData.at(0).first,0,'f',0).arg(newData.at(newData.size()-1).first,0,'f',0));

        d_attenData = newData;
        emit attenFileParseSuccess(true);
        return true;
    }

    emit attenFileParseSuccess(true);
    return true;
}
