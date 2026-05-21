#include "CommandSerializer.h"

#include <algorithm>

constexpr unsigned char ESCAPED_BYTE_SENTRY = 61;
constexpr unsigned char ESCAPED_60 = 44;
constexpr unsigned char ESCAPED_61 = 45;
constexpr unsigned char ESCAPED_62 = 46;

namespace CommandSerializer
{
	Buffer _escapeSpecials(const Buffer& src)
	{
		Buffer ret;
		ret.reserve(src.size());

		for (auto&& b : src)
		{
			switch (b)
			{
			case 60:
				ret.push_back(ESCAPED_BYTE_SENTRY);
				ret.push_back(ESCAPED_60);
				break;

			case 61:
				ret.push_back(ESCAPED_BYTE_SENTRY);
				ret.push_back(ESCAPED_61);
				break;

			case 62:
				ret.push_back(ESCAPED_BYTE_SENTRY);
				ret.push_back(ESCAPED_62);
				break;

			default:
				ret.push_back(b);
				break;
			}
		}

		return ret;
	}

	Buffer _unescapeSpecials(const Buffer& src)
	{
		Buffer ret;
		ret.reserve(src.size());

		for (size_t i = 0; i < src.size(); i++)
		{
			auto currByte = src[i];
			if (currByte == ESCAPED_BYTE_SENTRY)
			{
				if (i == src.size() - 1)
				{
					throw std::runtime_error("No data left for escaped byte data");
				}
				i = i + 1;
				switch (src[i])
				{
				case ESCAPED_60:
					ret.push_back(60);
					break;

				case ESCAPED_61:
					ret.push_back(61);
					break;

				case ESCAPED_62:
					ret.push_back(62);
					break;

				default:
					throw std::runtime_error("Unexpected escaped byte");
					break;
				}
			}
			else
			{
				ret.push_back(currByte);
			}
		}

		return ret;
	}

	unsigned char _sumChecksum(const char* src, size_t size)
	{
		unsigned char accumulator = 0;
		for (size_t i = 0; i < size; i++)
		{
			accumulator += src[i];
		}
		return accumulator;
	}

	unsigned char _sumChecksum(const Buffer& src)
	{
		return _sumChecksum(src.data(), src.size());
	}

	Buffer packageDataForBt(const Buffer& src, DATA_TYPE dataType, unsigned int seqNumber)
	{
		//Reserve at least the size for the size, start&end markers, and the source
		Buffer toEscape;
		toEscape.reserve(src.size() + 2 + sizeof(int));
		Buffer ret;
		ret.reserve(toEscape.capacity());
		toEscape.push_back(static_cast<unsigned char>(dataType));
		toEscape.push_back(seqNumber);
		auto retSize = intToBytesBE(static_cast<unsigned int>(src.size()));
		//Insert data size
		toEscape.insert(toEscape.end(), retSize.begin(), retSize.end());
		//Insert command data
		toEscape.insert(toEscape.end(), src.begin(), src.end());

		auto checksum = _sumChecksum(toEscape);
		toEscape.push_back(checksum);
		toEscape = _escapeSpecials(toEscape);

		
		ret.push_back(START_MARKER);
		ret.insert(ret.end(), toEscape.begin(), toEscape.end());
		ret.push_back(END_MARKER);


		// Message will be chunked if it's larger than MAX_BLUETOOTH_MESSAGE_SIZE, just crash to deal with it for now
		if (ret.size() > MAX_BLUETOOTH_MESSAGE_SIZE)
		{
			throw std::runtime_error("Exceeded the max bluetooth message size, and I can't handle chunked messages");
		}

		return ret;
	}

	static unsigned int _readSizeBE(const Buffer& unescaped)
	{
		if (unescaped.size() < 6) {
			throw std::runtime_error("Invalid message: missing size field");
		}
		return (static_cast<unsigned char>(unescaped[2]) << 24)
			| (static_cast<unsigned char>(unescaped[3]) << 16)
			| (static_cast<unsigned char>(unescaped[4]) << 8)
			| static_cast<unsigned char>(unescaped[5]);
	}

	Message unpackBtMessage(const Buffer& src)
	{
		//Message data format: ESCAPE_SPECIALS(<DATA_TYPE><SEQ_NUMBER><BIG ENDIAN 4 BYTE SIZE OF UNESCAPED DATA><DATA><1 BYTE CHECKSUM>)
		auto unescaped = _unescapeSpecials(src);

		if (unescaped.size() < 7)
		{
			throw std::runtime_error("Invalid message: Smaller than the minimum message size");
		}

		Message ret;
		ret.dataType = static_cast<DATA_TYPE>(unescaped[0]);
		ret.seqNumber = unescaped[1];
		const auto dataSize = _readSizeBE(unescaped);
		if (unescaped.size() < 6 + dataSize + 1)
		{
			throw std::runtime_error("Invalid message: payload shorter than declared size");
		}
		if ((unsigned char)unescaped[unescaped.size() - 1] != _sumChecksum(unescaped.data(), unescaped.size() - 1))
		{
			throw RecoverableException("Invalid checksum!", false);
		}
		ret.payload.assign(unescaped.begin() + 6, unescaped.begin() + 6 + dataSize);
		return ret;
	}

	NC_DUAL_SINGLE_VALUE getDualSingleForAsmLevel(char asmLevel, int maxAsmLevel)
	{
		NC_DUAL_SINGLE_VALUE val = NC_DUAL_SINGLE_VALUE::OFF;
		if (asmLevel > maxAsmLevel)
		{
			throw std::runtime_error("Exceeded max steps");
		}
		else if (asmLevel == 1)
		{
			val = NC_DUAL_SINGLE_VALUE::SINGLE;
		}
		else if (asmLevel == 0)
		{
			val = NC_DUAL_SINGLE_VALUE::DUAL;
		}
		return val;
	}

	Buffer serializeNcAndAsmSetting(NC_ASM_EFFECT ncAsmEffect, NC_ASM_SETTING_TYPE ncAsmSettingType, ASM_SETTING_TYPE asmSettingType, ASM_ID asmId, char asmLevel, int maxAsmLevel)
	{
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(COMMAND_TYPE::NCASM_SET_PARAM));
		ret.push_back(static_cast<unsigned char>(NC_ASM_INQUIRED_TYPE::NOISE_CANCELLING_AND_AMBIENT_SOUND_MODE));
		ret.push_back(static_cast<unsigned char>(ncAsmEffect));
		ret.push_back(static_cast<unsigned char>(ncAsmSettingType));
		ret.push_back(static_cast<unsigned char>(getDualSingleForAsmLevel(asmLevel, maxAsmLevel)));
		ret.push_back(static_cast<unsigned char>(asmSettingType));
		ret.push_back(static_cast<unsigned char>(asmId));
		ret.push_back(asmLevel);
		return ret;
	}

	Buffer serializeNcAndAsmAmbientLevelV1(NC_ASM_EFFECT effect, ASM_ID asmId, int ambientLevel)
	{
		const int level = std::max(0, std::min(ambientLevel, 255));
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(COMMAND_TYPE::NCASM_SET_PARAM));
		ret.push_back(static_cast<unsigned char>(NC_ASM_INQUIRED_TYPE::NOISE_CANCELLING_AND_AMBIENT_SOUND_MODE));
		ret.push_back(static_cast<unsigned char>(effect));
		ret.push_back(static_cast<unsigned char>(NC_ASM_SETTING_TYPE::DUAL_SINGLE_OFF));
		ret.push_back(static_cast<unsigned char>(NC_DUAL_SINGLE_VALUE::OFF));
		ret.push_back(static_cast<unsigned char>(ASM_SETTING_TYPE::LEVEL_ADJUSTMENT));
		ret.push_back(static_cast<unsigned char>(asmId));
		ret.push_back(static_cast<unsigned char>(level));
		return ret;
	}

	Buffer serializeNcAndAsmNcDualV1(NC_ASM_EFFECT effect, ASM_ID asmId)
	{
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(COMMAND_TYPE::NCASM_SET_PARAM));
		ret.push_back(static_cast<unsigned char>(NC_ASM_INQUIRED_TYPE::NOISE_CANCELLING_AND_AMBIENT_SOUND_MODE));
		ret.push_back(static_cast<unsigned char>(effect));
		ret.push_back(static_cast<unsigned char>(NC_ASM_SETTING_TYPE::DUAL_SINGLE_OFF));
		ret.push_back(static_cast<unsigned char>(NC_DUAL_SINGLE_VALUE::DUAL));
		ret.push_back(static_cast<unsigned char>(ASM_SETTING_TYPE::LEVEL_ADJUSTMENT));
		ret.push_back(static_cast<unsigned char>(asmId));
		ret.push_back(0);
		return ret;
	}

	Buffer serializeAmbientSoundControlV2(bool ambientEnabled, bool focusOnVoice, int asmLevel, bool windNoiseMode)
	{
		const unsigned char variant = windNoiseMode ? 0x17 : 0x15;
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(PAYLOAD_CMD::NCASM_SET));
		ret.push_back(variant);
		ret.push_back(0x01);

		if (!ambientEnabled) {
			ret.push_back(0x00);
			ret.push_back(0x00);
			if (windNoiseMode) {
				ret.push_back(0x02);
			}
			ret.push_back(focusOnVoice ? 0x01 : 0x00);
			ret.push_back(0x00);
			return ret;
		}

		ret.push_back(0x01);
		ret.push_back(0x01);
		if (windNoiseMode) {
			ret.push_back(0x02);
		}
		ret.push_back(focusOnVoice ? 0x01 : 0x00);
		ret.push_back(static_cast<char>(asmLevel));
		return ret;
	}

	Buffer serializeVPTSetting(VPT_INQUIRED_TYPE type, unsigned char preset)
	{
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(COMMAND_TYPE::VPT_SET_PARAM));
		ret.push_back(static_cast<unsigned char>(type));
		ret.push_back(preset);

		return ret;
	}

	Buffer serializePayload(const Buffer& payloadBytes)
	{
		return payloadBytes;
	}

	static unsigned char encodeEqLevel(int level)
	{
		const int clamped = std::max(EQ_LEVEL_MIN, std::min(EQ_LEVEL_MAX, level));
		return static_cast<unsigned char>(clamped + 10);
	}

	Buffer serializeEqualizerPreset(EQ_PRESET preset)
	{
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(PAYLOAD_CMD::EQ_SET));
		ret.push_back(0x01);
		ret.push_back(static_cast<unsigned char>(preset));
		ret.push_back(0x00);
		return ret;
	}

	Buffer serializeEqualizerWithBands(EQ_PRESET preset, int clearBass, const std::array<int, EQ_BAND_COUNT>& bands)
	{
		// Sony SongPal (zk.t) / Gadgetbridge: 58 01 [preset] [count] clearBass + 5 bands (each level + 10).
		// Band-step updates use preset UNSPECIFIED (0xff) after MANUAL mode is selected on the device.
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(PAYLOAD_CMD::EQ_SET));
		ret.push_back(0x01); // EqEbbInquiredType::PRESET_EQ
		ret.push_back(static_cast<unsigned char>(preset));
		ret.push_back(static_cast<unsigned char>(1 + EQ_BAND_COUNT));
		ret.push_back(static_cast<unsigned char>(encodeEqLevel(clearBass)));
		for (int band : bands) {
			ret.push_back(static_cast<unsigned char>(encodeEqLevel(band)));
		}
		return ret;
	}

	Buffer serializeTouchSensor(bool enabled)
	{
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(PAYLOAD_CMD::TOUCH_SET));
		ret.push_back(static_cast<unsigned char>(GS_INQUIRED_TYPE::GENERAL_SETTING2));
		ret.push_back(0x01);
		ret.push_back(enabled ? 0x01 : 0x00);
		return ret;
	}

	Buffer serializeVoiceGuidance(bool enabled)
	{
		// table2: SET 48, inquired 01, DetailedDataType::ON_OFF 01, value.
		Buffer ret;
		ret.push_back(static_cast<unsigned char>(VOICE_GUIDANCE_CMD::SET_PARAM));
		ret.push_back(static_cast<unsigned char>(VOICE_GUIDANCE_INQUIRED::VOICE_GUIDANCE_SETTING));
		ret.push_back(0x01);
		ret.push_back(enabled ? 0x01 : 0x00);
		return ret;
	}

}

