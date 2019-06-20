#include <leddevice/LedDeviceWrapper.h>

#include <leddevice/LedDevice.h>
#include <leddevice/LedDeviceFactory.h>

// following file is auto generated by cmake! it contains all available leddevice headers
#include "LedDevice_headers.h"

// util
#include <hyperion/Hyperion.h>
#include <utils/JsonUtils.h>

// qt
#include <QThread>
#include <QDir>

LedDeviceRegistry LedDeviceWrapper::_ledDeviceMap = LedDeviceRegistry();

LedDeviceWrapper::LedDeviceWrapper(Hyperion* hyperion)
	: QObject(hyperion)
	, _hyperion(hyperion)
	, _ledDevice(nullptr)
	, _enabled(true)
{
	// prepare the device constrcutor map
	#define REGISTER(className) LedDeviceWrapper::addToDeviceMap(QString(#className).toLower(), LedDevice##className::construct);

	// the REGISTER() calls are autogenerated by cmake.
	#include "LedDevice_register.cpp"

	#undef REGISTER

	_hyperion->setNewComponentState(hyperion::COMP_LEDDEVICE, true);
}

LedDeviceWrapper::~LedDeviceWrapper()
{
	stopDeviceThread();
}

void LedDeviceWrapper::createLedDevice(const QJsonObject& config)
{
	if(_ledDevice != nullptr)
	 {
		 stopDeviceThread();
	 }

	 // create thread and device
	 QThread* thread = new QThread(this);
	 _ledDevice = LedDeviceFactory::construct(config);
	 _ledDevice->moveToThread(thread);
	 // setup thread management
	 connect(thread, &QThread::started, _ledDevice, &LedDevice::start);
	 connect(thread, &QThread::finished, thread, &QObject::deleteLater);
	 connect(thread, &QThread::finished, _ledDevice, &QObject::deleteLater);

	 // further signals
	 connect(this, &LedDeviceWrapper::write, _ledDevice, &LedDevice::write);
	 connect(_ledDevice, &LedDevice::enableStateChanged, this, &LedDeviceWrapper::handleInternalEnableState);

	 // start the thread
	 thread->start();
}

const QJsonObject LedDeviceWrapper::getLedDeviceSchemas()
{
	// make sure the resources are loaded (they may be left out after static linking)
	Q_INIT_RESOURCE(LedDeviceSchemas);

	// read the json schema from the resource
	QDir d(":/leddevices/");
	QStringList l = d.entryList();
	QJsonObject result, schemaJson;

	for(QString &item : l)
	{
		QString schemaPath(QString(":/leddevices/")+item);
		QString devName = item.remove("schema-");

		QString data;
		if(!FileUtils::readFile(schemaPath, data, Logger::getInstance("LedDevice")))
		{
			throw std::runtime_error("ERROR: Schema not found: " + item.toStdString());
		}

		QJsonObject schema;
		if(!JsonUtils::parse(schemaPath, data, schema, Logger::getInstance("LedDevice")))
		{
			throw std::runtime_error("ERROR: Json schema wrong of file: " + item.toStdString());
		}

		schemaJson = schema;
		schemaJson["title"] = QString("edt_dev_spec_header_title");

		result[devName] = schemaJson;
	}

	return result;
}

int LedDeviceWrapper::addToDeviceMap(QString name, LedDeviceCreateFuncType funcPtr)
{
	_ledDeviceMap.emplace(name,funcPtr);
	return 0;
}

const LedDeviceRegistry& LedDeviceWrapper::getDeviceMap()
{
	return _ledDeviceMap;
}

int LedDeviceWrapper::getLatchTime()
{
	return _ledDevice->getLatchTime();
}

const QString & LedDeviceWrapper::getActiveDevice()
{
	return _ledDevice->getActiveDevice();
}

const QString & LedDeviceWrapper::getColorOrder()
{
	return _ledDevice->getColorOrder();
}

void LedDeviceWrapper::handleComponentState(const hyperion::Components component, const bool state)
{
	if(component == hyperion::COMP_LEDDEVICE)
	{
		_ledDevice->setEnable(state);
		_hyperion->setNewComponentState(hyperion::COMP_LEDDEVICE, _ledDevice->componentState());
		_enabled = state;
	}
}

void LedDeviceWrapper::handleInternalEnableState(bool newState)
{
	_hyperion->setNewComponentState(hyperion::COMP_LEDDEVICE, newState);
	_enabled = newState;
}

void LedDeviceWrapper::stopDeviceThread()
{
	_ledDevice->switchOff();
	QThread* oldThread = _ledDevice->thread();
	delete _ledDevice; // fast desctruction
	oldThread->quit(); // non blocking
	oldThread->wait();
}
