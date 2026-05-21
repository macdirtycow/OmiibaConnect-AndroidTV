#include "AndroidBluetoothConnector.h"

#include <android/log.h>
#include <cstring>

namespace {
constexpr const char* kLogTag = "OmiibaBt";
JavaVM* gJvm = nullptr;
} // namespace

void AndroidBluetoothConnector::setJavaVm(JavaVM* vm)
{
	gJvm = vm;
}

JavaVM* AndroidBluetoothConnector::javaVm()
{
	return gJvm;
}

AndroidBluetoothConnector::AndroidBluetoothConnector(jobject transportGlobalRef)
{
	JNIEnv* env = attach();
	transportRef_ = env->NewGlobalRef(transportGlobalRef);
	detach(env);
}

AndroidBluetoothConnector::~AndroidBluetoothConnector()
{
	if (transportRef_ == nullptr || gJvm == nullptr) {
		return;
	}
	JNIEnv* env = attach();
	env->DeleteGlobalRef(transportRef_);
	transportRef_ = nullptr;
	detach(env);
}

JNIEnv* AndroidBluetoothConnector::attach() const
{
	JNIEnv* env = nullptr;
	if (gJvm->AttachCurrentThread(&env, nullptr) != JNI_OK || env == nullptr) {
		throw std::runtime_error("JNI attach failed");
	}
	return env;
}

void AndroidBluetoothConnector::detach(JNIEnv* env) const
{
	(void)env;
}

int AndroidBluetoothConnector::send(char* buf, size_t length)
{
	std::lock_guard guard(jniMtx_);
	JNIEnv* env = attach();
	jbyteArray array = env->NewByteArray(static_cast<jsize>(length));
	env->SetByteArrayRegion(array, 0, static_cast<jsize>(length), reinterpret_cast<jbyte*>(buf));
	jclass clazz = env->GetObjectClass(transportRef_);
	jmethodID mid = env->GetMethodID(clazz, "send", "([B)I");
	const jint sent = env->CallIntMethod(transportRef_, mid, array);
	env->DeleteLocalRef(array);
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		detach(env);
		throw RecoverableException("Could not send to headphones", false);
	}
	detach(env);
	return static_cast<int>(sent);
}

int AndroidBluetoothConnector::recv(char* buf, size_t length)
{
	std::lock_guard guard(jniMtx_);
	JNIEnv* env = attach();
	jclass clazz = env->GetObjectClass(transportRef_);
	const jmethodID mid = env->GetMethodID(clazz, "recv", "(IZ)[B");
	const jint blocking = blockingRecv_.load() ? 1 : 0;
	jbyteArray array = static_cast<jbyteArray>(env->CallObjectMethod(
		transportRef_, mid, static_cast<jint>(length), blocking));
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		detach(env);
		return 0;
	}
	if (array == nullptr) {
		detach(env);
		return 0;
	}
	const jsize size = env->GetArrayLength(array);
	const jsize copy = size < static_cast<jsize>(length) ? size : static_cast<jsize>(length);
	if (copy > 0) {
		env->GetByteArrayRegion(array, 0, copy, reinterpret_cast<jbyte*>(buf));
	}
	env->DeleteLocalRef(array);
	detach(env);
	return static_cast<int>(copy);
}

void AndroidBluetoothConnector::connect(const std::string& addrStr)
{
	std::lock_guard guard(jniMtx_);
	JNIEnv* env = attach();
	jstring addr = env->NewStringUTF(addrStr.c_str());
	jclass clazz = env->GetObjectClass(transportRef_);
	jmethodID mid = env->GetMethodID(clazz, "connect", "(Ljava/lang/String;)V");
	env->CallVoidMethod(transportRef_, mid, addr);
	env->DeleteLocalRef(addr);
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		detach(env);
		throw RecoverableException("Could not open RFCOMM to headphones", false);
	}
	connected_.store(true);
	detach(env);
}

void AndroidBluetoothConnector::disconnect() noexcept
{
	try {
		std::lock_guard guard(jniMtx_);
		JNIEnv* env = attach();
		jclass clazz = env->GetObjectClass(transportRef_);
		jmethodID mid = env->GetMethodID(clazz, "disconnect", "()V");
		env->CallVoidMethod(transportRef_, mid);
		if (env->ExceptionCheck()) {
			env->ExceptionClear();
		}
		detach(env);
	} catch (...) {
		__android_log_print(ANDROID_LOG_WARN, kLogTag, "disconnect failed");
	}
	connected_.store(false);
}

bool AndroidBluetoothConnector::isConnected() noexcept
{
	try {
		std::lock_guard guard(jniMtx_);
		JNIEnv* env = attach();
		jclass clazz = env->GetObjectClass(transportRef_);
		jmethodID mid = env->GetMethodID(clazz, "isConnected", "()Z");
		const jboolean ok = env->CallBooleanMethod(transportRef_, mid);
		if (env->ExceptionCheck()) {
			env->ExceptionClear();
			detach(env);
			return false;
		}
		detach(env);
		return ok == JNI_TRUE;
	} catch (...) {
		return false;
	}
}

std::vector<BluetoothDevice> AndroidBluetoothConnector::getConnectedDevices()
{
	return {};
}

void AndroidBluetoothConnector::discardPendingReceive() noexcept
{
	try {
		std::lock_guard guard(jniMtx_);
		JNIEnv* env = attach();
		jclass clazz = env->GetObjectClass(transportRef_);
		jmethodID mid = env->GetMethodID(clazz, "discardPendingReceive", "()V");
		env->CallVoidMethod(transportRef_, mid);
		if (env->ExceptionCheck()) {
			env->ExceptionClear();
		}
		detach(env);
	} catch (...) {
	}
}

void AndroidBluetoothConnector::setBlockingRecv(bool blocking) noexcept
{
	blockingRecv_.store(blocking);
}

void AndroidBluetoothConnector::serviceTransport() noexcept
{
	try {
		std::lock_guard guard(jniMtx_);
		JNIEnv* env = attach();
		jclass clazz = env->GetObjectClass(transportRef_);
		jmethodID mid = env->GetMethodID(clazz, "serviceTransport", "()V");
		env->CallVoidMethod(transportRef_, mid);
		if (env->ExceptionCheck()) {
			env->ExceptionClear();
		}
		detach(env);
	} catch (...) {
	}
}
