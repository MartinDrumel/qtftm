#include "batchwidget.h"
#include "ui_batchwidget.h"
#include "singlescandialog.h"
#include <QFileDialog>
#include <QSettings>
#include <QApplication>
#include <QMessageBox>
#include <math.h>
#include <QTextStream>
#include <QRadioButton>
#include <QComboBox>
#include <QDialogButtonBox>

BatchWidget::BatchWidget(SingleScanWidget *ssw, QtFTM::BatchType type, QWidget *parent) :
     QWidget(parent),
    ui(new Ui::BatchWidget), d_type(type), d_numTests(1), d_calDisabled(false)
{
	ui->setupUi(this);

	//store scan settings
	batchSsw = new SingleScanWidget(this);
	batchSsw->setFromScan(ssw->toScan());
	batchSsw->setVisible(false);

	ui->batchInsertButton->setEnabled(false);
	ui->batchClearButton->setEnabled(false);
	ui->batchDeleteButton->setEnabled(false);
	ui->batchEditButton->setEnabled(false);
	ui->batchMoveUpButton->setEnabled(false);
	ui->batchMoveDownButton->setEnabled(false);
	ui->batchInsertCalButton->setEnabled(false);
	ui->batchSaveButton->setEnabled(false);
    ui->sortButton->setEnabled(false);

	//connect buttons to callback functions
	connect(ui->batchAddButton,&QAbstractButton::clicked,this,&BatchWidget::addButtonCallBack);
	connect(ui->batchInsertButton,&QAbstractButton::clicked,this,&BatchWidget::insertButtonCallBack);
	connect(ui->batchAddCalButton,&QAbstractButton::clicked,this,&BatchWidget::addCalCallBack);
	connect(ui->batchInsertCalButton,&QAbstractButton::clicked,this,&BatchWidget::insertCalCallBack);
	connect(ui->batchClearButton,&QAbstractButton::clicked,this,&BatchWidget::clearButtonCallBack);
	connect(ui->batchDeleteButton,&QAbstractButton::clicked,this,&BatchWidget::deleteRows);
	connect(ui->batchEditButton,&QAbstractButton::clicked,this,&BatchWidget::editScan);
	connect(ui->batchMoveUpButton,&QAbstractButton::clicked,this,&BatchWidget::moveRowsUp);
	connect(ui->batchMoveDownButton,&QAbstractButton::clicked,this,&BatchWidget::moveRowsDown);
    connect(ui->sortButton,&QAbstractButton::clicked,this,&BatchWidget::sortButtonCallBack);
	connect(ui->batchLoadButton,&QAbstractButton::clicked,this,&BatchWidget::parseFile);
	connect(ui->batchSaveButton,&QAbstractButton::clicked,this,&BatchWidget::saveFile);

	//prepare table view
	ui->batchTableView->setModel(&btm);
	ui->batchTableView->horizontalHeader()->show();
	ui->batchTableView->setSelectionBehavior(QAbstractItemView::SelectRows);

	//make model-related connections
	connect(&btm,&BatchTableModel::modelChanged,ui->batchTableView,&QTableView::resizeColumnsToContents);
	connect(&btm,&BatchTableModel::modelChanged,ui->batchTableView,&QTableView::resizeRowsToContents);
    connect(&btm,&BatchTableModel::modelChanged,this,&BatchWidget::toggleButtons);
	connect(&btm,&BatchTableModel::modelChanged,this,&BatchWidget::scansChanged);
	connect(&btm,&BatchTableModel::modelChanged,this,&BatchWidget::updateLabel);
	connect(ui->batchTableView->selectionModel(),&QItemSelectionModel::selectionChanged,
		   this,&BatchWidget::toggleButtons);

	//make the label for the estimated time
	timeLabel = new QLabel(this);
	timeLabel->setAlignment(Qt::AlignCenter);
    ui->rightLayout->addWidget(timeLabel,0,Qt::AlignCenter);

    sleepCheckBox = new QCheckBox(QString("Sleep when batch is complete"),this);
    sleepCheckBox->setToolTip(QString("If checked, the instrument will be put into sleep mode when the acquisition is complete.\nThis will stop pulses from being generated, and will turn off the gas flow controllers."));
    sleepCheckBox->setChecked(false);
    ui->rightLayout->addWidget(sleepCheckBox,0,Qt::AlignCenter);

	//show a time estimate
	updateLabel();
}

BatchWidget::~BatchWidget()
{
	delete ui;
}

bool BatchWidget::isEmpty()
{
    return btm.rowCount(QModelIndex()) == 0;
}

void BatchWidget::disableCalibration()
{
    d_calDisabled = true;
    ui->batchAddCalButton->setEnabled(false);
    ui->batchAddCalButton->setVisible(false);

    ui->batchInsertCalButton->setEnabled(false);
    ui->batchInsertCalButton->setVisible(false);
}

void BatchWidget::updateLabel()
{
    if(d_type == QtFTM::Amdor)
    {
        timeLabel->setText(QString("Cannot provide time estimate for AMDOR procedure."));
        return;
    }
	//get the time esimate and calculate hours, minutes, and seconds
	int repeats = 1;
	if(d_type == QtFTM::Categorize)
		repeats = d_numTests;
	int totalTime = btm.timeEstimate(d_type,repeats);

	QSettings set(QSettings::SystemScope,QApplication::organizationName(),QApplication::applicationName());
	QString name = QString("Batch Scan");
	QString key = QString("batchNum");
	if(d_type == QtFTM::DrCorrelation)
	{
		name = QString("DR Correlation");
		key = QString("drCorrNum");
	}
	else if(d_type == QtFTM::Categorize)
	{
		name = QString("Category Test");
		key = QString("catTestNum");
	}
	QDateTime dt = QDateTime::currentDateTime().addSecs(totalTime);
	int h = totalTime/3600;
	int m = (totalTime%3600)/60;
	int s = totalTime%60;

	QString text;
	text.append(QString("<table><th colspan=2>%1 Number %2</th></tr>").arg(name).arg(set.value(key,1).toInt()));
	text.append(QString("<tr><td colspan=2 align=\"center\">Time estimates</td></tr>"));
	text.append(QString("<tr><td>Total time: </td> <td> %1h %2m %3s</td></tr>").arg(h).arg(m).arg(s));
	text.append(QString("<tr><td>Completed at: </td> <td> %1</td></tr>").arg(dt.toString()));
	text.append(QString("</table>"));

	timeLabel->setText(text);
}

void BatchWidget::addButtonCallBack()
{
	//make scan and add it to model if it's valid (will be invalid if user hits cancel)
	Scan scan = buildScanFromDialog();
	if(scan.targetShots()>0)
		btm.addScan(scan);
}

void BatchWidget::insertButtonCallBack()
{
	if(ui->batchTableView->selectionModel()->selectedRows().size()!=1)
		return;

	//get row number from selection and build scan
	int row = ui->batchTableView->selectionModel()->selectedRows().at(0).row();
	Scan scan = buildScanFromDialog();
	if(scan.targetShots()>0)
		btm.addScan(scan,false,row);
}

void BatchWidget::addCalCallBack()
{
	//same as add scan, but set the cal flag to true
	Scan scan = buildScanFromDialog(true);
	if(scan.targetShots()>0)
		btm.addScan(scan,true);
}

void BatchWidget::insertCalCallBack()
{
	//same as insert scan, but set cal flag true
	if(ui->batchTableView->selectionModel()->selectedRows().size()!=1)
		return;

	int row = ui->batchTableView->selectionModel()->selectedRows().at(0).row();
	Scan scan = buildScanFromDialog(true);
	if(scan.targetShots()>0)
		btm.addScan(scan,true,row);
}

void BatchWidget::toggleButtons()
{
	//set which buttons should be enabled
	//get selection
	QModelIndexList l = ui->batchTableView->selectionModel()->selectedRows();
	bool c = isSelectionContiguous(l);

	//insert and edit buttons are only active when 1 rows is selected
	ui->batchInsertButton->setEnabled(l.size() == 1);
	ui->batchInsertCalButton->setEnabled(l.size() == 1);
	ui->batchEditButton->setEnabled(l.size() == 1);

	//delete button is active if 1 or more rows selected
	ui->batchDeleteButton->setEnabled(l.size() > 0);

	//move buttons only active if selection is contiguous
	ui->batchMoveUpButton->setEnabled(c);
	ui->batchMoveDownButton->setEnabled(c);

	//clear and save buttons only active if the table is not empty
	if(btm.rowCount(QModelIndex())>0)
	{
		ui->batchClearButton->setEnabled(true);
		ui->batchSaveButton->setEnabled(true);
	}
	else
	{
		ui->batchClearButton->setEnabled(false);
		ui->batchSaveButton->setEnabled(false);
	}

    //sort button should only be active if there are 3 or more entries
    if(btm.rowCount(QModelIndex())>2)
        ui->sortButton->setEnabled(true);
    else
        ui->sortButton->setEnabled(false);

    if(d_calDisabled)
    {
        ui->batchAddCalButton->setEnabled(false);
        ui->batchInsertCalButton->setEnabled(false);
    }
}

void BatchWidget::clearButtonCallBack()
{
	//pop up message box to prevent accidental click
	int result = QMessageBox::question(this,QString("Clear all rows"),QString("Are you sure you want to clear all scans from the table? This action cannot be undone."),QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

	if(result == QMessageBox::Yes)
		btm.clear();
}

void BatchWidget::deleteRows()
{
	//delete selected rows
	QModelIndexList list = ui->batchTableView->selectionModel()->selectedRows();
	if(!list.isEmpty())
		btm.removeScans(list);
}

void BatchWidget::editScan()
{
	QModelIndexList l = ui->batchTableView->selectionModel()->selectedRows();
	if(!l.size()==1)
		return;

	//get selected scan
	Scan s = btm.getScan(l.at(0).row()).first;

	//build a dialog, and set the values in the dialog to those of the selected scan
	SingleScanDialog d(this);
	SingleScanWidget *ssw = d.ssWidget();
	ssw->enableSkipTune(true);

    ssw->setFromScan(s);

	int ret = d.exec();

	if(ret == QDialog::Rejected)
		return;

	//set the appropriate values in the scan
    s = ssw->toScan();

	//update the model
	btm.updateScan(l.at(0).row(),s);

}

void BatchWidget::moveRowsUp()
{
	QModelIndexList l = ui->batchTableView->selectionModel()->selectedRows();

	if(l.isEmpty())
		return;

	//get the selected rows
	QList<int> sortList;
	for(int i=0; i<l.size(); i++)
		sortList.append(l.at(i).row());

	qSort(sortList);

	//make sure selection is contiguous
	if(sortList.size()>1 && sortList.at(sortList.size()-1) - sortList.at(0) != sortList.size()-1)
		return;

	btm.moveRows(sortList.at(0),sortList.at(sortList.size()-1),-1);
}

void BatchWidget::moveRowsDown()
{
	//same as moving up
	QModelIndexList l = ui->batchTableView->selectionModel()->selectedRows();

	if(l.isEmpty())
		return;

	QList<int> sortList;
	for(int i=0; i<l.size(); i++)
		sortList.append(l.at(i).row());

	qSort(sortList);

	if(sortList.size()>1 && sortList.at(sortList.size()-1) - sortList.at(0) != sortList.size()-1)
		return;

    btm.moveRows(sortList.at(0),sortList.at(sortList.size()-1),1);
}

void BatchWidget::sortButtonCallBack()
{
    QDialog d;
    d.setWindowTitle(QString("Batch Sort"));

    QVBoxLayout *vl = new QVBoxLayout(&d);

    QRadioButton *freqButton = new QRadioButton(QString("Sort by cavity frequency"),&d);
    QRadioButton *posButton = new QRadioButton(QString("Sort by cavity position"),&d);
    QRadioButton *calButton = new QRadioButton(QString("No sorting, distribute cal scans evenly"),&d);
    QComboBox *orderBox = new QComboBox(&d);
    orderBox->addItem(QString("Ascending"),true);
    orderBox->addItem(QString("Descending"),false);
    orderBox->setCurrentIndex(0);

    connect(calButton,&QAbstractButton::toggled,orderBox,&QWidget::setDisabled);

    calButton->setChecked(true);

    vl->addWidget(freqButton);
    vl->addWidget(posButton);
    vl->addWidget(calButton);
    vl->addWidget(orderBox);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,Qt::Horizontal,&d);
    connect(bb->button(QDialogButtonBox::Ok),&QAbstractButton::clicked,&d,&QDialog::accept);
    connect(bb->button(QDialogButtonBox::Cancel),&QAbstractButton::clicked,&d,&QDialog::reject);

    vl->addWidget(bb);

    d.setLayout(vl);

    if(d.exec() != QDialog::Accepted)
        return;

    BatchTableModel::SortOptions so = BatchTableModel::CalOnly;
    if(freqButton->isChecked())
        so = BatchTableModel::Frequency;
    if(posButton->isChecked())
        so = BatchTableModel::CavityPos;

    bool ascending = true;
    if(orderBox->currentIndex() == 1)
        ascending = false;

    QApplication::setOverrideCursor(Qt::BusyCursor);
    setEnabled(false);
    QApplication::processEvents();

    btm.sortByCavityPosition(so,ascending);

    setEnabled(true);
    QApplication::restoreOverrideCursor();
}

void BatchWidget::parseFile()
{
	//launch file dialog
	QString fileName = QFileDialog::getOpenFileName(this,QString("Choose File"),QString(),QString("Batch Files (*.ftb);;Text Files (*.txt);;All Files (*.*)"));

	if(fileName.isEmpty())
		return;

	QFile f(fileName);

	if(!f.open(QIODevice::ReadOnly))
		return;

	//set up default scan. this will be the template for new scans
	//if the file doesn't specify a setting, it will be set to the default value
    //the default scan is initialized to the current settings, or the latest scan that was manually added
    Scan defaultScan = batchSsw->toScan();

	//set up default calibration scan
	Scan calScan = btm.getLastCalScan();
	bool calScanSet = false;
	if(calScan.targetShots()>0) //there is a cal scan in the list already; we can use that to add more
		calScanSet = true;
	else //no calibration scan; set default parameters
		calScan = defaultScan;

	QList<QPair<Scan,bool> > scanList;

	while(!f.atEnd())
	{
		QString line = f.readLine().trimmed();

		if(line.startsWith(QChar('#')) || line.isEmpty()) //comment line or empty line; don't parse
			continue;

		//read a line. if the line ends with \, read another one
		if(line.endsWith(QChar('\\')))
		{
			line.remove(line.size()-1,1);
			bool done = false;
			while(!done)
			{
				QString nextLine = f.readLine().trimmed();
				if(nextLine.startsWith(QChar('#')) || nextLine.isEmpty())
					break;

				line.append(nextLine);
				if(line.endsWith(QChar('\\')))
					line.remove(line.size()-1,1); //will add another line next
				else
					done = true;
			}
		}

		if(line.startsWith(QChar('@'))) //Modify default scan
		{
			line.remove(0,1); //remove the @ character
			Scan s = parseLine(defaultScan, line);
			if(s.targetShots()>0)
			{
				defaultScan = s;
				scanList.append(QPair<Scan,bool>(defaultScan,false));
			}

		}
		else if(line.startsWith(QChar('!'))) //Modify calibration scan
		{
            if(d_calDisabled)
                continue;

			line.remove(0,1); //remove the ! character
			Scan s = parseLine(calScan, line);
			if(s.targetShots()>0)
			{
				calScan = s;
				calScanSet = true;
			}
		}
		else if(line.startsWith(QString("cal"),Qt::CaseInsensitive)) //add calibration scan
		{
            if(d_calDisabled)
                continue;

			//only add the calibration scan if the template has been explicitly set
			if(calScanSet)
				scanList.append(QPair<Scan,bool>(calScan,true));
		}
        else if(line.startsWith(QString("amdor")))
        {
            //for AMDOR scans only, read this as a DR only test
            parseAmdorLine(line,defaultScan.drPower());
        }
		else
		{
			Scan s = parseLine(defaultScan,line);
			if(s.targetShots()>0)
				scanList << QPair<Scan,bool>(s,false);
		}
	} //end file parsing

	//add the scans
	btm.addScans(scanList);
}


void BatchWidget::saveFile()
{
	if(btm.rowCount(QModelIndex())==0)
		return;

	//launch save file dialog
	QString name = QFileDialog::getSaveFileName(this,QString("Save batch"),QString(),QString("Batch Files (*.ftb)"));

	if(name.isEmpty())
		return;

	QFile saveFile(name);
	if(!saveFile.open(QIODevice::WriteOnly))
	{
		QMessageBox::critical(this,QString("File error"),QString("The file you selected cannot be opened for writing. The batch file has NOT been saved. Please try again."));
		return;
	}

	QTextStream out(&saveFile);
	out << QString("#File created by QtFTM\n");

	Scan calScan;
	Scan refScan;

	//loop through rows
	for(int i=0; i<btm.rowCount(QModelIndex()); i++)
	{
		//get scan, and whether it's a calibration scan
		Scan thisScan = btm.getScan(i).first;
		bool isCal = btm.getScan(i).second;

		if(isCal)
		{
			//if it's the same as the most recent cal scan, just add "cal"
			if(calScan == thisScan)
				out << QString("\ncal");
			else
			{
				//write the full scan details
				calScan = thisScan;
				out << QString("\n!") << writeScan(thisScan) << QString("\ncal");
			}
		}
		else
		{
			//if the ref scan is invalid, set this one to the ref and write full details
			if(!refScan.targetShots()>0)
			{
				refScan = thisScan;
				out << QString("\n@") << writeScan(thisScan);
			}
			else //otherwise, write the differences from the ref scan
				out << QString("\n") << writeScan(thisScan,refScan);
		}
	}
    if(d_type == QtFTM::Amdor)
    {
        out.setRealNumberNotation(QTextStream::FixedNotation);
        out.setRealNumberPrecision(3);
        emit requestDrOnlyList();
        for(int i=0; i<d_amdorDrOnlyList.size(); i++)
        {
            out << QString("\namdor drfreq:") << d_amdorDrOnlyList.at(i).first
                << QString(" drpower:") << d_amdorDrOnlyList.at(i).second;
        }

    }
	out.flush();
	saveFile.close();
}

Scan BatchWidget::buildScanFromDialog(bool cal)
{
	//launch single scan dialog...
	SingleScanDialog d(this);
	d.setWindowTitle(QString("Single Scan"));

	SingleScanWidget *ssw = d.ssWidget();

	if(d_type == QtFTM::DrCorrelation || d_type == QtFTM::Categorize)
	{
		ssw->enableSkipTune(false);
		if(!cal && d_type == QtFTM::DrCorrelation)
			ssw->limitFtmRangeToDr();
	}
	else
		ssw->enableSkipTune(true);

	if(cal)
		ssw->shotsSpinBox()->blockSignals(true);

	if(cal && btm.getLastCalScan().targetShots()>0)
        ssw->setFromScan(btm.getLastCalScan());
	else
        ssw->setFromScan(batchSsw->toScan());

	int ret = d.exec();

	if(ret == QDialog::Rejected)
		return Scan();

	if(!cal)
        batchSsw->setFromScan(ssw->toScan());

    Scan scan = ssw->toScan();

	return scan;
}

bool BatchWidget::isSelectionContiguous(QModelIndexList l)
{
	if(l.isEmpty())
		return false;

	if(l.size()==1)
		return true;

	//selection is contiguous if the last row minus first row is equal to the size, after the list has been sorted
	QList<int> sortList;
	for(int i=0; i<l.size(); i++)
		sortList.append(l.at(i).row());

	qSort(sortList);

	if(sortList.at(sortList.size()-1) - sortList.at(0) != sortList.size()-1)
		return false;

	return true;
}

Scan BatchWidget::parseLine(Scan defaultScan, QString line)
{
	Scan out(defaultScan);
	QSettings s(QSettings::SystemScope,QApplication::organizationName(),QApplication::applicationName());

	//if we don't successfully parse anything on this line, we will return an invalid scan
	bool parseSuccess = false;

	//make a copy of the pulse configuration. it will be set again at the end, whether or not it has changed
    PulseGenConfig pConfig = defaultScan.pulseConfiguration();

	//split the line on whitespace characters, and iterate over it
	QStringList items = line.split(QRegExp(QString("\\s+")),QString::SkipEmptyParts);
	for(int i=0; i<items.size(); i++)
	{
		QString item = items.at(i).trimmed();
		if(item.isEmpty()) //this shouldn't happen, but whatever
			continue;

		//each item is a key-value pair, separated by a colon
		QStringList keyVal = item.split(QChar(':'));

		if(keyVal.size() != 2) // a valid entry will split into 2 pieces
			continue;

		//store key and value
		QString key = keyVal.at(0);
		QString val = keyVal.at(1);

		if(key.startsWith(QString("f"),Qt::CaseInsensitive)) //FTM Frequency
		{
			bool success = false;
			double d = val.toDouble(&success);
			if(success)
			{
				s.beginGroup(QString("ftmSynth"));
                s.beginGroup(s.value(QString("subKey"),QString("virtual")).toString());
                if(d >= s.value(QString("min"),5000.0).toDouble() && d <= s.value(QString("max"),43000.0).toDouble())
				{
					parseSuccess = true;
					out.setFtFreq(d);
				}
				s.endGroup();
                s.endGroup();
			}
		}
		else if(key.startsWith(QString("sh"),Qt::CaseInsensitive)) //shots
		{
			bool success = false;
			int shots = val.toInt(&success);
			if(success && shots > 0)
			{
				parseSuccess = true;
				out.setTargetShots(shots);
			}
		}
		else if(key.startsWith(QString("a"),Qt::CaseInsensitive)) //attenuation
		{
			bool success = false;
			int attn = val.toInt(&success);
			if(success)
			{
                s.beginGroup(QString("attn"));
                s.beginGroup(s.value(QString("subKey"),QString("virtual")).toString());
				if(attn >= s.value(QString("min"),0).toInt() && attn <= s.value(QString("max"),70).toInt())
				{
					parseSuccess = true;
					out.setAttenuation(attn);
				}
				s.endGroup();
                s.endGroup();
			}
		}
		else if(key.startsWith(QString("drf"),Qt::CaseInsensitive)) //dr frequency
		{
			bool success = false;
			double f = val.toDouble(&success);
			if(success)
			{
				s.beginGroup(QString("drSynth"));
                s.beginGroup(s.value(QString("subKey"),QString("virtual")).toString());
                if(f >= s.value(QString("min"),1000.0).toDouble() && f <= s.value(QString("max"),1000000.0).toDouble())
				{
					parseSuccess = true;
					out.setDrFreq(f);
				}
				s.endGroup();
                s.endGroup();
			}
		}
		else if(key.startsWith(QString("drp"),Qt::CaseInsensitive)) //dr power
		{
			bool success = false;
			double p = val.toDouble(&success);
			if(success)
			{
				s.beginGroup(QString("drSynth"));
                s.beginGroup(s.value(QString("subKey"),QString("virtual")).toString());
                if(p >= s.value(QString("minPower"),-70.0).toDouble() && p <= s.value(QString("maxPower"),17.0).toDouble())
				{
					parseSuccess = true;
					out.setDrPower(p);
				}
				s.endGroup();
                s.endGroup();
			}
		}
		else if(key.startsWith(QString("dcv"),Qt::CaseInsensitive)) //dc voltage
		{
			bool success = false;
			int v = val.toInt(&success);
			if(success)
			{
				s.beginGroup(QString("hvps"));
				s.beginGroup(s.value(QString("subKey"),QString("virtual")).toString());
				if(v >= s.value(QString("min"),0).toInt() && v <= s.value(QString("max"),2000).toInt())
				{
					parseSuccess = true;
					out.setDcVoltage(v);
				}
				s.endGroup();
				s.endGroup();
			}
		}
		else if(key.startsWith(QString("p"),Qt::CaseInsensitive)) //pulse configuration
		{
			//the pulse config key has 3 parts, the pulse identifier, the channel, and the setting, separated by commas
			QStringList subkeys = key.split(QChar(','));
			if(subkeys.size() == 3) //valid entries have 3 parts
			{
				bool chSuccess = false;
				int ch = subkeys.at(1).toInt(&chSuccess);
				if(chSuccess && ch >= 0 && ch < pConfig.size()) //channel selected
				{
					s.beginGroup(QString("pulseGenerator"));
					s.beginGroup(s.value(QString("subKey"),QString("virtual")).toString());
					if(subkeys.at(2).startsWith(QString("w"))) //width
					{
						bool success = false;
						double w = val.toDouble(&success);
						if(success)
						{
							if(w >= s.value(QString("minWidth"),0.004).toDouble()
                                    && w <= s.value(QString("maxWidth"),100000.0).toDouble())
							{
								parseSuccess = true;
                                pConfig.set(ch,QtFTM::PulseWidth,w);
							}
						}
					}
					if(subkeys.at(2).startsWith(QString("d"))) //delay
					{
						bool success = false;
						double delay = val.toDouble(&success);
						if(success)
						{
							if(delay >= s.value(QString("minDelay"),0.0).toDouble()
									&& delay <= s.value(QString("maxDelay"),100000.0).toDouble())
							{
								parseSuccess = true;
                                pConfig.set(ch,QtFTM::PulseDelay,delay);
							}
						}
					}
					if(subkeys.at(2).startsWith(QString("a"))) //active level
					{
						bool success = false;
						int level = val.toInt(&success);
						if(success)
						{
							parseSuccess = true;
							if(level == 0 && ch != QTFTM_PGEN_MWCHANNEL)
								pConfig.set(ch,QtFTM::PulseLevel,
										  QVariant::fromValue(QtFTM::PulseLevelActiveHigh));
							else
								pConfig.set(ch,QtFTM::PulseLevel,
										  QVariant::fromValue(QtFTM::PulseLevelActiveLow));
						}
						else
						{
							if(val.contains(QString("high"),Qt::CaseInsensitive))
							{
								parseSuccess = true;
								pConfig.set(ch,QtFTM::PulseLevel,
										  QVariant::fromValue(QtFTM::PulseLevelActiveHigh));
							}
							else if(val.contains(QString("low"),Qt::CaseInsensitive))
							{
								parseSuccess = true;
								pConfig.set(ch,QtFTM::PulseLevel,
										  QVariant::fromValue(QtFTM::PulseLevelActiveLow));
							}
						}
					}
                    if(subkeys.at(2).startsWith(QString("e")) && ch != QTFTM_PGEN_GASCHANNEL && ch != QTFTM_PGEN_MWCHANNEL) //enabled; can't disable gas or mw!
					{
						bool success = false;
						int enabled = val.toInt(&success);
						if(success)
						{
							parseSuccess = true;
							if(enabled == 0)
                                pConfig.set(ch,QtFTM::PulseEnabled,false);
							else
                                pConfig.set(ch,QtFTM::PulseEnabled,true);
						}
						else
						{
							if(val.contains(QString("yes"),Qt::CaseInsensitive) ||
									val.contains(QString("on"),Qt::CaseInsensitive) ||
									val.contains(QString("true"),Qt::CaseInsensitive))
							{
								parseSuccess = true;
                                pConfig.set(ch,QtFTM::PulseEnabled,true);
							}
							else if(val.contains(QString("no"),Qt::CaseInsensitive) ||
								   val.contains(QString("off"),Qt::CaseInsensitive) ||
								   val.contains(QString("false"),Qt::CaseInsensitive))
							{
								parseSuccess = true;
                                pConfig.set(ch,QtFTM::PulseEnabled,false);
							}
						}
					}
					s.endGroup();
                    s.endGroup();
				}
			}
		}

        else if(key.startsWith(QString("delp"),Qt::CaseInsensitive)) //protection delay
        {
            bool success = false;
            int delayp = val.toInt(&success);
            if(success)
            {
                s.beginGroup(QString("pdg"));
                s.beginGroup(s.value(QString("subKey"),QString("virtual")).toString());
                if(delayp >= s.value(QString("minProt"),1).toInt() && delayp <= s.value(QString("maxProt"),100).toInt())
                {
                    parseSuccess = true;
                    out.setProtectionDelayTime(delayp);
                }
                s.endGroup();
                s.endGroup();
            }
        }

        else if(key.startsWith(QString("dels"),Qt::CaseInsensitive)) //scope delay
        {
            bool success = false;
            int delays = val.toInt(&success);
            if(success)
            {
                s.beginGroup(QString("pdg"));
                s.beginGroup(s.value(QString("subKey"),QString("virtual")).toString());
                if(delays >= s.value(QString("minScope"),0).toInt() && delays <= s.value(QString("maxScope"),100).toInt())
                {
                    parseSuccess = true;
                    out.setScopeDelayTime(delays);
                }
                s.endGroup();
                s.endGroup();
            }
        }

        else if(key.startsWith(QString("dipo"),Qt::CaseInsensitive)) //This is the dipole's moment - when is yours??
        {
            bool success = false;
            double dipole = val.toDouble(&success);
            if(success)
            {
//                s.beginGroup(QString("dipoleMoment"));
                if(dipole >= s.value(QString("minDipole"),0).toDouble() && dipole <= s.value(QString("maxDipole"),10.0).toDouble())
                {
                    parseSuccess = true;
                    out.setDipoleMoment(dipole);
                }
//                s.endGroup();
            }
        }
		else if(key.startsWith(QString("m"),Qt::CaseInsensitive))
		{
			bool success = false;
			bool m = (bool)val.toInt(&success);
			if(success)
			{
				parseSuccess = true;
				out.setMagnet(m);
			}
			else
			{
				if(val.contains(QString("yes"),Qt::CaseInsensitive) ||
						val.contains(QString("on"),Qt::CaseInsensitive) ||
						val.contains(QString("true"),Qt::CaseInsensitive))
				{
					parseSuccess = true;
					out.setMagnet(true);
				}
				else if(val.contains(QString("no"),Qt::CaseInsensitive) ||
					   val.contains(QString("off"),Qt::CaseInsensitive) ||
					   val.contains(QString("false"),Qt::CaseInsensitive))
				{
					parseSuccess = true;
					out.setMagnet(false);
				}
			}
		}
		else if(key.startsWith(QString("sk"),Qt::CaseInsensitive))
		{
			bool success = false;
			bool skip = (bool)val.toInt(&success);
			if(success)
			{
				parseSuccess = true;
				out.setSkiptune(skip);
			}
			else
			{
				if(val.contains(QString("yes"),Qt::CaseInsensitive) ||
						val.contains(QString("on"),Qt::CaseInsensitive) ||
						val.contains(QString("true"),Qt::CaseInsensitive))
				{
					parseSuccess = true;
					out.setSkiptune(true);
				}
				else if(val.contains(QString("no"),Qt::CaseInsensitive) ||
					   val.contains(QString("off"),Qt::CaseInsensitive) ||
					   val.contains(QString("false"),Qt::CaseInsensitive))
				{
					parseSuccess = true;
					out.setSkiptune(false);
				}
			}
		}


	}

	out.setPulseConfiguration(pConfig);

	if(parseSuccess)
		return out;
	else
        return Scan();
}

void BatchWidget::parseAmdorLine(QString line, double drp)
{
    QSettings s(QSettings::SystemScope,QApplication::organizationName(),QApplication::applicationName());
    double drFreq = -1.0;
    double drPower = drp;
    bool parseSuccess = false;

    QStringList items = line.split(QRegExp(QString("\\s+")),QString::SkipEmptyParts);
    for(int i=0; i<items.size(); i++)
    {
        QString item = items.at(i).trimmed();
        if(item.isEmpty()) //this shouldn't happen, but whatever
            continue;

        //each item is a key-value pair, separated by a colon
        QStringList keyVal = item.split(QChar(':'));

        if(keyVal.size() != 2) // a valid entry will split into 2 pieces
            continue;

        //store key and value
        QString key = keyVal.at(0);
        QString val = keyVal.at(1);

        if(key.startsWith(QString("drf"),Qt::CaseInsensitive)) //dr frequency
        {
            bool success = false;
            double f = val.toDouble(&success);
            if(success)
            {
                s.beginGroup(QString("drSynth"));
                s.beginGroup(s.value(QString("subKey"),QString("virtual")).toString());
                if(f >= s.value(QString("min"),1000.0).toDouble() && f <= s.value(QString("max"),1000000.0).toDouble())
                {
                    parseSuccess = true;
                    drFreq = f;
                }
                s.endGroup();
                s.endGroup();
            }
        }
        else if(key.startsWith(QString("drp"),Qt::CaseInsensitive)) //dr power
        {
            bool success = false;
            double p = val.toDouble(&success);
            if(success)
            {
                s.beginGroup(QString("drSynth"));
                s.beginGroup(s.value(QString("subKey"),QString("virtual")).toString());
                if(p >= s.value(QString("minPower"),-70.0).toDouble() && p <= s.value(QString("maxPower"),17.0).toDouble())
                {
                    parseSuccess = true;
                    drPower = p;
                }
                s.endGroup();
                s.endGroup();
            }
        }
    }

    if(parseSuccess && drFreq > 0.0)
        emit amdorDrOnly(drFreq,drPower);
}

QString BatchWidget::writeScan(Scan thisScan, Scan ref)
{
	bool writeAll = false;
	if(!ref.targetShots()>0) // only write settings that are different from ref
		writeAll = true;

	QString out;
	QTextStream t(&out);

	if(thisScan.ftFreq() != ref.ftFreq() || writeAll)
		t << QString("ftm:") << QString::number(thisScan.ftFreq(),'f',4) << QString(" ");

	if(thisScan.targetShots() != ref.targetShots() || writeAll)
		t << QString("shots:") << thisScan.targetShots() << QString(" ");

    if(thisScan.attenuation() != ref.attenuation() || writeAll)
        t << QString("atten:") << thisScan.attenuation() << QString(" ");

	if(thisScan.drFreq() != ref.drFreq() || writeAll)
		t << QString("drfreq:") << QString::number(thisScan.drFreq(),'f',4) << QString(" ");

	if(thisScan.drPower() != ref.drPower() || writeAll)
		t << QString("drpower:") << QString::number(thisScan.drPower(),'f',1) << QString(" ");

	if(thisScan.dcVoltage() != ref.dcVoltage() || writeAll)
		t << QString("dcvoltage:") << thisScan.dcVoltage() << QString(" ");

	PulseGenConfig thispConfig = thisScan.pulseConfiguration();
	PulseGenConfig refpConfig = ref.pulseConfiguration();

	for(int i=0; i<thispConfig.size(); i++)
	{
		QtFTM::PulseChannelConfig pc = thispConfig.at(i);
		QtFTM::PulseChannelConfig pref;
		if(i<refpConfig.size())
			pref = refpConfig.at(i);

		//Do not write gas pulse width, or enabled settings for gas or mw (ch 0 and 2)
		if((pc.width != pref.width || writeAll) && i != QTFTM_PGEN_GASCHANNEL && i!= QTFTM_PGEN_MWCHANNEL)
			t << QString("pulse,") << i << QString(",width:") << QString::number(pc.width,'f',3) << QString(" ");

		if(pc.delay != pref.delay || writeAll)
			t << QString("pulse,") << i << QString(",delay:") << QString::number(pc.delay,'f',3) << QString(" ");

		if(pc.level != pref.level || writeAll)
		{
			if(pc.level == QtFTM::PulseLevelActiveHigh)
				t << QString("pulse,") << i << QString(",active:high ");
			else
				t << QString("pulse,") << i << QString(",active:low ");
		}

		if((pc.enabled != pref.enabled || writeAll) && i != QTFTM_PGEN_GASCHANNEL && i!= QTFTM_PGEN_MWCHANNEL)
		{
			if(pc.enabled)
				t << QString("pulse,") << i << QString(",enabled:true ");
			else
				t << QString("pulse,") << i << QString(",enabled:false ");
		}
	}

    if(thisScan.protectionDelayTime() != ref.protectionDelayTime()|| writeAll)
        t << QString("delpro:") << thisScan.protectionDelayTime() << QString(" ");

    if(thisScan.scopeDelayTime() != ref.scopeDelayTime() || writeAll)
        t << QString("delscope:") << thisScan.scopeDelayTime() << QString(" ");

    if(thisScan.dipoleMoment() != ref.dipoleMoment() || writeAll)
        t << QString("dipole:") << QString::number(thisScan.dipoleMoment() ,'f',2) << QString(" ");

    if(thisScan.magnet() != ref.magnet() || writeAll)
    {
	    if(thisScan.magnet())
		    t << QString("magnet:true ");
	    else
		    t << QString("magnet:false ");
    }

    if(thisScan.skipTune() != ref.skipTune() || writeAll)
    {
	    if(thisScan.skipTune())
		    t << QString("skiptune:true ");
	    else
		    t << QString("skiptune:false ");
    }

	t.flush();
	return out;
}
