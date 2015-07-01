#ifndef HARDWAREOBJECT_H
#define HARDWAREOBJECT_H

#include <QObject>
#include <QString>
#include "datastructs.h"
#include <QSettings>
#include <QApplication>

/*!
 * \brief Abstract base class for all hardware connected to the instrument.
 *
 * This class establishes a common interface for all hardware.
 * Adding a new piece of hardware to the code involves creating a subclass of HardwareObject (or of one of the existing subclasses: see TcpInstrument, GpibInstrument, and Rs232Instrument)
 * Each hardware object has a name (d_prettyName and name()) that is used in the user interface, and a key (d_key and key()) that is used to refer to the object in the settings file.
 * Subclasses must assign strings to these variables in their constructors.
 * For communication with the UI, a logMessage() signal exists that can print a message in the log with an optional status code.
 * In general, this should only be used for reporting errors, but it is also useful in debugging.
 *
 * HardwareObjects are designed to be used in their own threads for the most part.
 * Because of this, the constructor should generally not be called with a parent, and any QObjects that will be created in the child class with this as the parent should NOT be initialized in the constructor.
 * Generally, a HardwareObject is created, signal-slot connections are made, and then the object is pushed into its own thread.
 * The thread's started() signal is connected to the initialize pure virtual slot, which must be implemented for a child class.
 * Any functions that need to be called from outside the class (e.g., from the HardwareManager during a scan) should be declared as slots, and QMetaObject::invokeMethod used to activate them (optionally with the Qt::BlockingQueuedConnection flag set if it returns a value or other actions need to wait until it is complete).
 * If for some reason this is not possible, then data members need to be protected by a QMutex to prevent concurrent access.
 * In the initialize function, any QObjects can be created and other settings made, because the object is already in its new thread.
 * The initialize function must call testConnection() before it returns.
 *
 * The testConnection() slot should attempt to establish communication with the physical device and perform some sort of test to make sure that the connection works and that the correct device is targeted.
 * Usually, this takes the form of an ID query of some type (e.g., *IDN?).
 * After determining if the connection is successful, the testConnection() function MUST emit the connectionResult() signal with the this pointer, a boolean indicating success, and an optional error message that can be displayed in the log along with the failure notification.
 * The testConnection() slot is also called from the communication dialog to re-test a connection that is lost.
 *
 * If at any point during operation, the program loses communication with the device, the hardwareFailure() signal should be emitted with the this pointer.
 * When emitted, any ongoing scan will be terminated and all controls will be frozen until communication is re-established by a successful call to the testConnection() function.
 * Some communication options (timeout, termination chartacters, etc) can be set, but it is up to the subclass to decide how or whether to make use of them.
 * They are provided here for convenience because TCP Instruments and RS232 instruments both use them.
 *
 * Finally, the sleep() function may be implemented in a subclass if anything needs to be done to put the device into an inactive state.
 * For instance, the FlowController turns off gas flows to conserve sample, and the PulseGenerator turns off all pulses.
 * If sleep() is implemented, it is recommneded to explicitly call HardwareObject::sleep(), as this will display a message in the log stating that the device is in fact asleep.
 */
class HardwareObject : public QObject
{
	Q_OBJECT
public:
    /*!
     * \brief Constructor. Does nothing.
     *
     * \param parent Pointer to parent QObject. Should be 0 if it will be in its own thread.
     */
	explicit HardwareObject(QString key, QString name, QObject *parent = nullptr);

    /*!
     * \brief Access function for pretty name.
     * \return Name for display on UI
     */
	QString name() { return d_prettyName; }

    /*!
     * \brief Access function for key.
     * \return Name for use in the settings file
     */
	QString key() { return d_key; }
	
signals:
    /*!
     * \brief Displays a message on the log.
     * \param QString The message to display
     * \param QtFTM::LogMessageCode The status incidator (Normal, Warning, Error, Highlight)
     */
	void logMessage(const QString, const QtFTM::LogMessageCode = QtFTM::LogNormal);

    /*!
     * \brief Indicates whether a connection is successful
     * \param HardwareObject* This pointer
     * \param bool True if connection is successful
     * \param msg If unsuccessful, an optional message to display on the log tab along with the failure message.
     */
    void connected(bool success = true,QString msg = QString());

    /*!
     * \brief Signal emitted if communication to hardware is lost.
     * \param HardwareObject* This pointer
     */
    void hardwareFailure();
	
public slots:
    /*!
     * \brief Attempt to communicate with hardware. Must emit connectionResult(). Pure virtual.
     * \return Whether attempt was successful
     */
	virtual bool testConnection() =0;

    /*!
     * \brief Do any needed initialization prior to connecting to hardware. Pure virtual
     *
     */
	virtual void initialize() =0;

    /*!
     * \brief Puts device into a standby mode. Default implementation puts a message in the log.
     * \param b If true, go into standby mode. Else, active mode.
     */
	virtual void sleep(bool b);

protected:
    const QString d_prettyName; /*!< Name to be displayed on UI */
    const QString d_key; /*!< Name to be used in settings */

    QByteArray d_readTerminator; /*!< Termination characters that indicate a message from the device is complete. */
    bool d_useTermChar; /*!< If true, a read operation is complete when the message ends with d_readTerminator */
    int d_timeOut; /*!< Timeout for read operation, in ms */

    /*!
     * \brief Convenience function for setting read options
     * \param tmo Read timeout, in ms
     * \param useTermChar If true, look for termination characters at the end of a message
     * \param termChar Termination character(s)
     */
    void setReadOptions(int tmo, bool useTermChar = false, QByteArray termChar = QByteArray()) { d_timeOut = tmo, d_useTermChar = useTermChar, d_readTerminator = termChar; }
	
};

#endif // HARDWAREOBJECT_H
