#ifndef VIRTUALMOTORDRIVER_H
#define VIRTUALMOTORDRIVER_H

#include "motordriver.h"

class VirtualMotorDriver : public MotorDriver
{
public:
	explicit VirtualMotorDriver(QObject *parent = nullptr);
	~VirtualMotorDriver();

	// HardwareObject interface
public slots:
	bool testConnection();
	void initialize();

	// MotorDriver interface
protected:
	bool moveToPosition(int pos);
	bool stepMotor(int motorSteps);
	int readAnalog();
	int readPos();

	// MotorDriver interface
public slots:
	void tune(double freq, int currentAttn, int mode);
	void calibrate();
};

#endif // VIRTUALMOTORDRIVER_H
