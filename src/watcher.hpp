#pragma once

#include <filesystem>
#include <functional>
#include <cassert>
#include <iostream>
#include <chrono>

class Watcher {
public:
	Watcher(std::filesystem::path filepath, std::function<void(void)> on_write_f)
		: m_filepath(std::move(filepath))
		, m_on_write_f(on_write_f)
	{
		assert(std::filesystem::exists(m_filepath));
		m_prev_write_time = std::filesystem::last_write_time(m_filepath);
		m_prev_check_time = std::chrono::steady_clock::now();
	}
	
	void update()
	{
		using namespace std::chrono_literals;

		if (auto now = std::chrono::steady_clock::now()
				; m_prev_check_time + 100ms < now
				&& std::filesystem::exists(m_filepath)) {
			m_prev_check_time = now;

			
			if (auto write_time = std::filesystem::last_write_time(m_filepath)
					; write_time != m_prev_write_time) {
				m_prev_write_time = write_time;
				m_on_write_f();
			}
		}
	}

private:
	std::filesystem::path m_filepath;
	std::filesystem::file_time_type m_prev_write_time;
	std::function<void(void)> m_on_write_f;
	std::chrono::steady_clock::time_point m_prev_check_time;
};
