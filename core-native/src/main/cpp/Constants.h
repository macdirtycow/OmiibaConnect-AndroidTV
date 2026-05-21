#pragma once

#include <vector>

inline constexpr auto MAX_BLUETOOTH_MESSAGE_SIZE = 2048;
inline constexpr char START_MARKER{ 62 };
inline constexpr char END_MARKER{ 60 };

inline constexpr auto MAC_ADDR_STR_SIZE = 17;

inline constexpr auto SERVICE_UUID = "96CC203E-5068-46ad-B32D-E316F5E069BA";
inline unsigned char SERVICE_UUID_IN_BYTES[] = { // this is the SERVICE_UUID but in bytes
	0x96, 0xcc, 0x20, 0x3e, 0x50, 0x68, 0x46, 0xad,
	0xb3, 0x2d, 0xe3, 0x16, 0xf5, 0xe0, 0x69, 0xba
};

#define APP_NAME "Omiiba Connect v" __HEADPHONES_APP_VERSION__
#define APP_NAME_W (L"" APP_NAME)

using Buffer = std::vector<char>;

enum class DATA_TYPE : signed char
{
	DATA = 0,
	ACK = 1,
    DATA_MC_NO1 = 2,
    DATA_ICD = 9,
    DATA_EV = 10,
	DATA_MDR = 12,
    DATA_COMMON = 13,
    DATA_MDR_NO2 = 14,
    SHOT =  16,
    SHOT_MC_NO1 =  18,
    SHOT_ICD =  25,
    SHOT_EV =  26,
    SHOT_MDR =  28,
    SHOT_COMMON =  29,
    SHOT_MDR_NO2 = 30,
    LARGE_DATA_COMMON =  45,
    UNKNOWN = -1
};


enum class NC_ASM_INQUIRED_TYPE : signed char
{
	NO_USE = 0,
	NOISE_CANCELLING = 1,
	NOISE_CANCELLING_AND_AMBIENT_SOUND_MODE = 2,
	AMBIENT_SOUND_MODE = 3
};

enum class NC_ASM_EFFECT : signed char
{
	OFF = 0,
	ON = 1,
	ADJUSTMENT_IN_PROGRESS = 16,
	ADJUSTMENT_COMPLETION = 17
};

enum class NC_ASM_SETTING_TYPE : signed char
{
	ON_OFF = 0,
	LEVEL_ADJUSTMENT = 1,
	DUAL_SINGLE_OFF = 2
};

enum class ASM_SETTING_TYPE : signed char
{
	ON_OFF = 0,
	LEVEL_ADJUSTMENT = 1
};

enum class ASM_ID : signed char
{
	NORMAL = 0,
	VOICE = 1
};

enum class NC_DUAL_SINGLE_VALUE : signed char
{
	OFF = 0,
	SINGLE = 1,
	DUAL = 2
};

enum class COMMAND_TYPE : signed char
{
	VPT_GET_PARAM = 70,
	VPT_SET_PARAM = 72,
	NCASM_GET_PARAM = 102,
	NCASM_SET_PARAM = 104
};

// Sony MDR v1 payload command bytes — from Sony APK (com.sony.songpal.mdr) via
// com.sony.songpal.tandemfamily.message.mdr and community docs (mdr-protocol, Gadgetbridge).
enum class PAYLOAD_CMD : unsigned char
{
	// CONNECT category — must be sent before other queries (Sony app always starts here).
	CONNECT_GET_PROTOCOL_INFO = 0x00,
	CONNECT_RET_PROTOCOL_INFO = 0x01,
	CONNECT_GET_CAPABILITY_INFO = 0x02,
	CONNECT_RET_CAPABILITY_INFO = 0x03,
	CONNECT_GET_DEVICE_INFO = 0x04,
	CONNECT_RET_DEVICE_INFO = 0x05,
	CONNECT_GET_SUPPORT_FUNCTION = 0x06,
	CONNECT_RET_SUPPORT_FUNCTION = 0x07,

	BATTERY_REQUEST = 0x10,
	BATTERY_RET = 0x11,
	BATTERY_NTFY = 0x13,
	AUDIO_CODEC_REQUEST = 0x18,
	AUDIO_CODEC_RET = 0x19,
	AUDIO_CODEC_NTFY = 0x1b,
	// Alias: device info uses CONNECT_GET_DEVICE_INFO + DeviceInfoInquiredType
	FW_VERSION_REQUEST = CONNECT_GET_DEVICE_INFO,
	FW_VERSION_RET = CONNECT_RET_DEVICE_INFO,
	SOUND_GET = 0x46,
	SOUND_RET = 0x47,
	SOUND_SET = 0x48,
	EQ_GET = 0x56,
	EQ_RET = 0x57,
	EQ_SET = 0x58,
	// NCASM_* = Noise Cancelling / Ambient Sound Mode (APK: NCASM_GET_PARAM 0x66, RET 0x67, SET 0x68).
	NCASM_GET = 0x66,
	NCASM_RET = 0x67,
	NCASM_SET = 0x68,
	TOUCH_GET = 0xd6,
	TOUCH_RET = 0xd7,
	TOUCH_SET = 0xd8,
	VOICE_GET = 0x46,
	VOICE_RET = 0x47,
	VOICE_SET = 0x48,

	// OPT — NC optimizer (APK: OPT_SET_STATUS)
	NC_OPTIMIZER_SET = 0x84,
	NC_OPTIMIZER_NTFY = 0x85,

	// AUDIO — DSEE HX / upsampling (APK: AUDIO_GET_PARAM)
	AUDIO_UPSAMPLING_GET = 0xe6,
	AUDIO_UPSAMPLING_RET = 0xe7,
	AUDIO_UPSAMPLING_SET = 0xe8,

	// SYSTEM — auto power-off, button modes (APK: SYSTEM_GET_PARAM)
	SYSTEM_GET_PARAM = 0xf6,
	SYSTEM_RET_PARAM = 0xf7,
	SYSTEM_SET_PARAM = 0xf8,
};

// CONNECT_GET_DEVICE_INFO sub-types (DeviceInfoInquiredType in APK enums).
enum class DEVICE_INFO_TYPE : unsigned char
{
	MODEL_NAME = 0x01,
	FW_VERSION = 0x02,
	SERIES_AND_COLOR = 0x03,
};

// GENERAL_SETTING_GET_PARAM sub-type (GsInquiredType in APK).
enum class GS_INQUIRED_TYPE : signed char
{
	GENERAL_SETTING1 = static_cast<signed char>(0xD1),
	GENERAL_SETTING2 = static_cast<signed char>(0xD2),
	GENERAL_SETTING3 = static_cast<signed char>(0xD3),
};

// table2 voice guidance (Command.java v1/table2).
enum class VOICE_GUIDANCE_CMD : unsigned char
{
	GET_PARAM = 0x46,
	RET_PARAM = 0x47,
	SET_PARAM = 0x48,
};

enum class VOICE_GUIDANCE_INQUIRED : unsigned char
{
	VOICE_GUIDANCE_SETTING = 0x01,
};

enum class EQ_PRESET : unsigned char
{
	OFF = 0x00,
	BRIGHT = 0x10,
	EXCITED = 0x11,
	MELLOW = 0x12,
	RELAXED = 0x13,
	VOCAL = 0x14,
	TREBLE_BOOST = 0x15,
	BASS_BOOST = 0x16,
	SPEECH = 0x17,
	MANUAL = 0xa0,
	// Band-step writes use UNSPECIFIED on the wire (Sony SongPal / Gadgetbridge), not MANUAL.
	UNSPECIFIED = 0xff,
	USER_PROFILE_1 = 0xa1,
	USER_PROFILE_2 = 0xa2,
	USER_PROFILE_3 = 0xa3,
};

constexpr int EQ_BAND_COUNT = 5;
constexpr int EQ_LEVEL_MIN = -10;
constexpr int EQ_LEVEL_MAX = 10;

inline bool eqPresetUsesBandPayload(EQ_PRESET preset)
{
	return preset == EQ_PRESET::MANUAL
		|| preset == EQ_PRESET::USER_PROFILE_1
		|| preset == EQ_PRESET::USER_PROFILE_2
		|| preset == EQ_PRESET::USER_PROFILE_3;
}

enum class VPT_PRESET_ID : signed char
{
	OFF = 0,
	OUTDOOR_FESTIVAL = 1,
	ARENA = 2,
	CONCERT_HALL = 3,
	CLUB = 4
	//Note: Sony reserved 5~15 "for future"
};

enum class SOUND_POSITION_PRESET : signed char
{
	OFF = 0,
	FRONT_LEFT = 1,
	FRONT_RIGHT = 2,
	FRONT = 3,
	REAR_LEFT = 17,
	REAR_RIGHT = 18,
	OUT_OF_RANGE = -1
};

//Needed for converting the ImGui Combo index into the VPT index.
inline const SOUND_POSITION_PRESET SOUND_POSITION_PRESET_ARRAY[] = {
	SOUND_POSITION_PRESET::OFF,
	SOUND_POSITION_PRESET::FRONT_LEFT,
	SOUND_POSITION_PRESET::FRONT_RIGHT,
	SOUND_POSITION_PRESET::FRONT,
	SOUND_POSITION_PRESET::REAR_LEFT,
	SOUND_POSITION_PRESET::REAR_RIGHT,
	SOUND_POSITION_PRESET::OUT_OF_RANGE
};

enum class VPT_INQUIRED_TYPE : signed char
{
	NO_USE = 0,
	VPT = 1,
	SOUND_POSITION = 2,
	OUT_OF_RANGE = -1
};
