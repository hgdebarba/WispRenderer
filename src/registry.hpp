#pragma once

#include <map>

#include "util/log.hpp"

#undef GetObject // Prevent macro collision

namespace wr
{
	using RegistryHandle = std::uint64_t;
}

namespace wr::internal
{
	template<typename C, typename TO, typename TD>
	class Registry
	{
	protected:
		Registry() : m_next_handle(0) { }

	public:
		virtual ~Registry() = default;

		TO* Find(RegistryHandle handle)
		{
			auto it = m_objects.find(handle);
			if (it != m_objects.end())
			{
				return (*it).second;
			}
			else
			{
				LOGE("Failed to find registry object related to registry handle.");
				return nullptr;
			}
		}

		static C& Get()
		{
			static C* instance = new C();
			return *instance;
			//static C instance;
			//return instance;
		}

		std::map<RegistryHandle, TD> m_descriptions;
		std::map<RegistryHandle, TO*> m_objects;

	protected:
		RegistryHandle m_next_handle;
	};

} /* wr::internal */