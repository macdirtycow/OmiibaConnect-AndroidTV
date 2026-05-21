#pragma once

#include "IBluetoothConnector.h"

#include <atomic>
#include <jni.h>
#include <mutex>
#include <vector>

/** RFCOMM transport implemented via JNI calls into Kotlin BluetoothRfcommTransport. */
class AndroidBluetoothConnector final : public IBluetoothConnector {
public:
	static void setJavaVm(JavaVM* vm);
	static JavaVM* javaVm();

	explicit AndroidBluetoothConnector(jobject transportGlobalRef);
	~AndroidBluetoothConnector() override;

	int send(char* buf, size_t length) noexcept(false) override;
	int recv(char* buf, size_t length) noexcept(false) override;
	void connect(const std::string& addrStr) noexcept(false) override;
	void disconnect() noexcept override;
	bool isConnected() noexcept override;
	std::vector<BluetoothDevice> getConnectedDevices() override;

	void discardPendingReceive() noexcept override;
	void setBlockingRecv(bool blocking) noexcept override;
	void serviceTransport() noexcept override;

private:
	jobject transportRef_ = nullptr;
	std::atomic<bool> connected_{false};
	std::atomic<bool> blockingRecv_{true};
	std::mutex jniMtx_;

	JNIEnv* attach() const;
	void detach(JNIEnv* env) const;
};
