#include "log_ui.h"
#include "engine/log.h"
#include "imgui/imgui.h"


LogUI::LogUI(Lumix::IAllocator& allocator)
	: m_allocator(allocator)
	, m_messages(allocator)
	, m_current_tab(0)
	, m_notifications(allocator)
	, m_last_uid(1)
	, m_guard(false)
{
	m_is_opened = false;
	Lumix::g_log_info.getCallback().bind<LogUI, &LogUI::onInfo>(this);
	Lumix::g_log_error.getCallback().bind<LogUI, &LogUI::onError>(this);
	Lumix::g_log_warning.getCallback().bind<LogUI, &LogUI::onWarning>(this);

	for (int i = 0; i < Count; ++i)
	{
		m_new_message_count[i] = 0;
		m_messages.emplace(allocator);
	}
}


LogUI::~LogUI()
{
	Lumix::g_log_info.getCallback().unbind<LogUI, &LogUI::onInfo>(this);
	Lumix::g_log_error.getCallback().unbind<LogUI, &LogUI::onError>(this);
	Lumix::g_log_warning.getCallback().unbind<LogUI, &LogUI::onWarning>(this);
}


void LogUI::setNotificationTime(int uid, float time)
{
	for (auto& notif : m_notifications)
	{
		if (notif.uid == uid)
		{
			notif.time = time;
			break;
		}
	}
}


int LogUI::addNotification(const char* text)
{
	m_move_notifications_to_front = true;
	auto& notif = m_notifications.emplace(m_allocator);
	notif.time = 10.0f;
	notif.message = text;
	notif.uid = ++m_last_uid;
	return notif.uid;
}


void LogUI::push(Type type, const char* message)
{
	Lumix::MT::SpinLock lock(m_guard);
	++m_new_message_count[type];
	m_messages[type].push(Lumix::string(message, m_allocator));

	if (type == Error)
	{
		addNotification(message);
	}
}


void LogUI::onInfo(const char* system, const char* message)
{
	push(Info, message);
}


void LogUI::onWarning(const char* system, const char* message)
{
	push(Warning, message);
}


void LogUI::onError(const char* system, const char* message)
{
	push(Error, message);
}


void fillLabel(char* output, int max_size, const char* label, int count)
{
	Lumix::copyString(output, max_size, label);
	Lumix::catString(output, max_size, "(");
	int len = Lumix::stringLength(output);
	Lumix::toCString(count, output + len, max_size - len);
	Lumix::catString(output, max_size, ")###");
	Lumix::catString(output, max_size, label);
}


void LogUI::showNotifications()
{
	m_are_notifications_hovered = false;
	if (m_notifications.empty()) return;

	ImGui::SetNextWindowPos(ImVec2(10, 30));
	bool opened;
	if (!ImGui::Begin("Notifications",
			&opened,
			ImVec2(200, 0),
			1.0f,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_ShowBorders))
	{
		ImGui::End();
		return;
	}

	m_are_notifications_hovered = ImGui::IsWindowHovered();

	if (ImGui::Button("Close")) m_notifications.clear();

	if (m_move_notifications_to_front) ImGui::BringToFront();
	m_move_notifications_to_front = false;
	for (int i = 0; i < m_notifications.size(); ++i)
	{
		if (i > 0) ImGui::Separator();
		ImGui::Text("%s", m_notifications[i].message.c_str());
	}
	ImGui::End();
}


void LogUI::update(float time_delta)
{
	if (m_are_notifications_hovered) return;

	for (int i = 0; i < m_notifications.size(); ++i)
	{
		m_notifications[i].time -= time_delta;

		if (m_notifications[i].time < 0)
		{
			m_notifications.erase(i);
			--i;
		}
	}
}


int LogUI::getUnreadErrorCount() const
{
	return m_new_message_count[Error];
}


void LogUI::onGUI()
{
	Lumix::MT::SpinLock lock(m_guard);
	showNotifications();

	if (ImGui::BeginDock("Log", &m_is_opened))
	{
		const char* labels[] = { "Info", "Warning", "Error" };
		for (int i = 0; i < Lumix::lengthOf(labels); ++i)
		{
			char label[40];
			fillLabel(label, sizeof(label), labels[i], m_new_message_count[i]);
			if(i > 0) ImGui::SameLine();
			if (ImGui::Button(label))
			{
				m_current_tab = i;
				m_new_message_count[i] = 0;
			}
		}
		
		auto* messages = &m_messages[m_current_tab];

		if (ImGui::Button("Clear"))
		{
			for (int i = 0; i < m_messages.size(); ++i)
			{
				m_messages[m_current_tab].clear();
				m_new_message_count[m_current_tab] = 0;
			}
		}

		ImGui::SameLine();
		char filter[128] = "";
		ImGui::InputText("Filter", filter, sizeof(filter));

		if (ImGui::BeginChild("log_messages"))
		{
			for (int i = 0; i < messages->size(); ++i)
			{
				const char* msg = (*messages)[i].c_str();
				if (filter[0] == '\0' || strstr(msg, filter) != nullptr)
				{
					ImGui::Text("%s", msg);
				}
			}
		}
		ImGui::EndChild();
	}
	ImGui::EndDock();
}
