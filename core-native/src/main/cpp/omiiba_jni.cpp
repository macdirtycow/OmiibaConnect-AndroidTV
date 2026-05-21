#include "AndroidBluetoothConnector.h"
#include "Headphones.h"

#include <android/log.h>
#include <jni.h>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace {
std::mutex gSessionMtx;
std::unique_ptr<BluetoothWrapper> gBt;
std::unique_ptr<Headphones> gHeadphones;

Headphones& requireHeadphones()
{
	if (!gHeadphones) {
		throw std::runtime_error("Not connected");
	}
	return *gHeadphones;
}

void throwToJava(JNIEnv* env, const std::exception& exc)
{
	jclass clazz = env->FindClass("java/lang/IllegalStateException");
	if (clazz != nullptr) {
		env->ThrowNew(clazz, exc.what());
	}
}
} // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*)
{
	AndroidBluetoothConnector::setJavaVm(vm);
	return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeBindTransport(JNIEnv* env, jclass, jobject transport)
{
	std::lock_guard guard(gSessionMtx);
	gHeadphones.reset();
	gBt.reset();
	auto connector = std::make_unique<AndroidBluetoothConnector>(transport);
	gBt = std::make_unique<BluetoothWrapper>(std::move(connector));
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeConnect(
	JNIEnv* env, jclass, jstring deviceName, jstring mac)
{
	try {
		std::lock_guard guard(gSessionMtx);
		if (!gBt) {
			throw std::runtime_error("Transport not bound");
		}
		const char* macChars = env->GetStringUTFChars(mac, nullptr);
		gBt->connect(macChars);
		env->ReleaseStringUTFChars(mac, macChars);

		gHeadphones = std::make_unique<Headphones>(*gBt);
		const char* nameChars = env->GetStringUTFChars(deviceName, nullptr);
		gHeadphones->configureForDevice(nameChars);
		env->ReleaseStringUTFChars(deviceName, nameChars);
		gHeadphones->performConnectHandshake();
		gHeadphones->refreshFromDevice(true);
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeDisconnect(JNIEnv*, jclass)
{
	std::lock_guard guard(gSessionMtx);
	gHeadphones.reset();
	if (gBt) {
		gBt->disconnect();
	}
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeIsConnected(JNIEnv*, jclass)
{
	std::lock_guard guard(gSessionMtx);
	return gBt && gBt->isConnected() && gHeadphones ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativePrepareForControl(JNIEnv* env, jclass)
{
	try {
		requireHeadphones().prepareForControl();
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeProbe(JNIEnv* env, jclass)
{
	try {
		return requireHeadphones().probeConnection() ? JNI_TRUE : JNI_FALSE;
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
		return JNI_FALSE;
	}
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeRefresh(JNIEnv* env, jclass, jboolean extended)
{
	try {
		requireHeadphones().refreshFromDevice(extended == JNI_TRUE);
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeApplyPending(JNIEnv* env, jclass)
{
	try {
		Headphones& hp = requireHeadphones();
		hp.prepareForControl();
		for (int i = 0; i < 3 && hp.hasPendingVirtualSoundChanges(); i++) {
			hp.setVirtualSoundChangesIfNeeded();
			if (i < 2) {
				std::this_thread::sleep_for(std::chrono::milliseconds(80));
			}
		}
		hp.resyncAmbientAfterVirtualSoundIfNeeded();
		hp.setAmbientChangesIfNeeded();
		hp.setEqChangesIfNeeded();
		hp.setTouchAndVoiceChangesIfNeeded();
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeSetAmbient(
	JNIEnv* env, jclass, jboolean enabled, jboolean focusOnVoice, jint asmLevel)
{
	try {
		Headphones& hp = requireHeadphones();
		hp.setAmbientSoundControl(enabled == JNI_TRUE);
		hp.setFocusOnVoice(focusOnVoice == JNI_TRUE);
		hp.setAsmLevel(static_cast<int>(asmLevel));
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeSetVpt(JNIEnv* env, jclass, jint vpt)
{
	try {
		Headphones& hp = requireHeadphones();
		hp.setVptType(static_cast<int>(vpt));
		hp.setSurroundPosition(SOUND_POSITION_PRESET::OFF);
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeSetSoundPosition(JNIEnv* env, jclass, jint preset)
{
	try {
		Headphones& hp = requireHeadphones();
		hp.setVptType(0);
		hp.setSurroundPosition(static_cast<SOUND_POSITION_PRESET>(static_cast<signed char>(preset)));
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeSetEqPreset(JNIEnv* env, jclass, jint preset)
{
	try {
		requireHeadphones().setEqualizerPreset(static_cast<EQ_PRESET>(preset));
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeSetEqManual(
	JNIEnv* env, jclass, jint bass, jintArray bands)
{
	try {
		std::array<int, EQ_BAND_COUNT> bandValues{};
		const jsize len = env->GetArrayLength(bands);
		if (len == EQ_BAND_COUNT) {
			env->GetIntArrayRegion(bands, 0, len, bandValues.data());
		}
		requireHeadphones().setEqualizerWithBands(EQ_PRESET::MANUAL, static_cast<int>(bass), bandValues);
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeSetTouchSensor(JNIEnv* env, jclass, jboolean enabled)
{
	try {
		requireHeadphones().setTouchSensorEnabled(enabled == JNI_TRUE);
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT void JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeSetVoiceGuidance(JNIEnv* env, jclass, jboolean enabled)
{
	try {
		requireHeadphones().setVoiceGuidanceEnabled(enabled == JNI_TRUE);
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
	}
}

extern "C" JNIEXPORT jobject JNICALL
Java_dev_omiiba_connect_tv_native_HeadphonesNative_nativeGetState(JNIEnv* env, jclass)
{
	try {
		const Headphones& hp = requireHeadphones();
		const auto& status = hp.getDeviceStatus();
		const auto& caps = hp.getCapabilities();
		jclass stateClass = env->FindClass("dev/omiiba/connect/tv/native/DeviceState");
		jmethodID ctor = env->GetMethodID(
			stateClass,
			"<init>",
			"(IIZIIIIIIZZZZZZZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
		std::string model = status.modelName;
		std::string codec = status.hasCodec ? status.audioCodec : "—";
		std::string fw = status.hasFirmware ? status.firmwareVersion : "—";
		jstring jModel = env->NewStringUTF(model.c_str());
		jstring jCodec = env->NewStringUTF(codec.c_str());
		jstring jFw = env->NewStringUTF(fw.c_str());
		return env->NewObject(
			stateClass,
			ctor,
			static_cast<jint>(hp.getDisplayAsmLevel()),
			static_cast<jint>(caps.asmMaxLevel),
			hp.getAmbientSoundControl() ? 1 : 0,
			hp.getFocusOnVoice() ? 1 : 0,
			static_cast<jint>(hp.getDisplayVptType()),
			static_cast<jint>(static_cast<int>(hp.getDisplaySurroundPosition())),
			static_cast<jint>(static_cast<int>(hp.getDisplayEqPreset())),
			hp.getDisplayEqBass(),
			status.hasBattery ? status.batteryPercent : -1,
			caps.supportsVirtualSound ? 1 : 0,
			caps.supportsEqualizer ? 1 : 0,
			caps.supportsTouchSensor ? 1 : 0,
			caps.supportsVoiceGuidance ? 1 : 0,
			hp.getTouchSensorEnabled() ? 1 : 0,
			hp.getVoiceGuidanceEnabled() ? 1 : 0,
			caps.usesV2AmbientSound ? 1 : 0,
			jModel,
			jCodec,
			jFw);
	} catch (const std::exception& exc) {
		throwToJava(env, exc);
		return nullptr;
	}
}
