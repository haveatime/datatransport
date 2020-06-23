#ifndef DT__LOCALBUS__PLUGIN__H
#define DT__LOCALBUS__PLUGIN__H

#include "datatransportinterface.h"
#include <QUdpSocket>
#include <QNetworkInterface> 
#include <QList> 

#ifdef DT_LOCALBUS_PLUGIN_EXPORT
#define DT_LOCALBUS_PLUGIN_DECL Q_DECL_EXPORT 
#else
#define DT_LOCALBUS_PLUGIN_DECL Q_DECL_IMPORT 
#endif

#define LOCAL_BUS_USE_LIMITED_BROADCAST     1   /* 使用受限广播 */
#define LOCAL_BUS_USE_LOOPBACK_MULTICAST    2   /* 使用环回组播 */
#define LOCAL_BUS_USE_NETWROK_MULTICAST     3   /* 使用默认组播 */

class DT_LOCALBUS_PLUGIN_DECL DtLocalBusFactory: public QObject, public DataTransportFactory
{
	Q_OBJECT

	Q_INTERFACES(DataTransportFactory)
#if QT_VERSION>=QT_VERSION_CHECK(5,0,0)
	Q_PLUGIN_METADATA(IID "tools.datatransportfactory.DataTransportFactory" FILE "dtlocalbusfactory.json")
#endif

public:
	virtual ~DtLocalBusFactory() {}

	virtual QSharedPointer<QObject> createOneDataTransportInstance( const QVariantHash &parameter );
};

class DT_LOCALBUS_PLUGIN_DECL DtLocalBusPlugin : public DataTransportInterface
{
	Q_OBJECT

private:
	QHostAddress groupaddress;
	int groupport;
	int groupmode;
	QUdpSocket groupsocket;

	QNetworkInterface getLoopBackInterface();

public:
	DtLocalBusPlugin( const QVariantHash &parameter );
	virtual ~DtLocalBusPlugin();

public slots:
	void processDatagrams();
	void processErrors(QAbstractSocket::SocketError socketError);

public slots:
	virtual void writeData(const QList<QByteArray> &data);
};

#endif
