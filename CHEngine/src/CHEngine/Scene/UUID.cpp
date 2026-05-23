#include "chepch.h"
#include "UUID.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/container_hash/hash.hpp>
#include <cstring>

namespace {
	constexpr auto GenerateReverseHexTable() {
		std::array<uint8_t, 256> table{};

		// Сначала заполняем всё невалидным маркером 0xFF
		for (size_t i = 0; i < 256; ++i) {
			table[i] = 0xFF;
		}

		// Заполняем цифры '0' - '9'
		for (char c = '0'; c <= '9'; ++c) {
			table[static_cast<uint8_t>(c)] = static_cast<uint8_t>(c - '0');
		}

		// Заполняем буквы 'A' - 'F'
		for (char c = 'A'; c <= 'F'; ++c) {
			table[static_cast<uint8_t>(c)] = static_cast<uint8_t>(10 + (c - 'A'));
		}

		// Заполняем буквы 'a' - 'f'
		for (char c = 'a'; c <= 'f'; ++c) {
			table[static_cast<uint8_t>(c)] = static_cast<uint8_t>(10 + (c - 'a'));
		}

		return table;
	}
	constexpr std::array<uint8_t, 256> ReverseHexTable = GenerateReverseHexTable();

	inline constexpr int HexToNibbleFast(char c) noexcept
	{
		return ReverseHexTable[static_cast<uint8_t>(c)];
	}
} // namespace

namespace CHEngine {

	UUID::UUID() noexcept
	{
		std::memset(m_Data, 0, 16);
	}

	UUID UUID::Generate()
	{
		thread_local boost::uuids::random_generator gen;

		const boost::uuids::uuid buu = gen();
		UUID result;
		std::memcpy(result.m_Data, buu.data, 16);
		return result;
	}

	UUID UUID::Nil() noexcept
	{
		return UUID{};
	}

	UUID UUID::FromString(std::string_view input)
	{
		std::string_view s = input;

		// Снимаем фигурные скобки, если они есть
		if (s.size() == 38 && s.front() == '{' && s.back() == '}')
			s = s.substr(1, s.size() - 2);

		// Валидный UUID без скобок должен быть либо 36 символов (с дефисами), либо 32 (без них)
		if (s.size() != 36 && s.size() != 32) return Nil();

		UUID result;
		size_t outIdx = 0;

		// Парсим строку напрямую без выделения памяти в std::string
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s[i] == '-') continue;

			// Защита от выхода за границы, если дефисы стояли не там
			if (outIdx >= 16 || i + 1 >= s.size()) return Nil();

			const int hi = HexToNibbleFast(s[i]);
			const int lo = HexToNibbleFast(s[i + 1]);

			if (hi == 0xFF || lo == 0xFF) return Nil();

			result.m_Data[outIdx++] = static_cast<uint8_t>((hi << 4) | lo);
			++i; // Шагаем через обработанный второй символ
		}

		if (outIdx != 16) return Nil();
		return result;
	}

	std::string UUID::ToString() const
	{
		boost::uuids::uuid buu;
		std::memcpy(buu.data, m_Data, 16);
		return boost::uuids::to_string(buu);
	}

	bool UUID::IsValid() const noexcept
	{
		const uint64_t* low = reinterpret_cast<const uint64_t*>(m_Data);
		return (low[0] != 0) || (low[1] != 0);
	}

	bool UUID::operator==(const UUID& o) const noexcept
	{
		const uint64_t* a = reinterpret_cast<const uint64_t*>(m_Data);
		const uint64_t* b = reinterpret_cast<const uint64_t*>(o.m_Data);
		return (a[0] == b[0]) && (a[1] == b[1]);
	}

	bool UUID::operator!=(const UUID& o) const noexcept
	{
		return !(*this == o);
	}

	bool UUID::operator<(const UUID& o) const noexcept
	{
		return std::memcmp(m_Data, o.m_Data, 16) < 0;
	}

	std::size_t hash_value(const UUID& u)
	{
		// Хэшируем два uint64_t напрямую, без создания объектов boost::uuid
		const uint64_t* data = reinterpret_cast<const uint64_t*>(u.Data());
		std::size_t seed = 0;
		boost::hash_combine(seed, data[0]);
		boost::hash_combine(seed, data[1]);
		return seed;
	}

} // namespace CHEngine