#include "ftsynthsettingswidget.h"

FtSynthSettingsWidget::FtSynthSettingsWidget(QWidget *parent) :
     SynthSettingsWidget(parent)
{
	d_title = QString("FTM Stynthesizer Settings");
	d_key = QString("ftmSynth");

	loadSettings();
}
