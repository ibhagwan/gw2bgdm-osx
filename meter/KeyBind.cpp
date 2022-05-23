#include "KeyBind.h"
#include <Windows.h>


static bool isVkDown(int vk)
{
	return (GetAsyncKeyState(vk) & 0x8000) ? true : false;
}

// Returns true if the given bind is down.
bool KeyBind::isKeyDown() const
{
	if (m_vk)
		return
		((m_ctrl == 0 && !isVkDown(VK_CONTROL)) || (m_ctrl == 1 && isVkDown(VK_CONTROL))) &&
		((m_alt == 0 && !isVkDown(VK_MENU)) || (m_alt == 1 && isVkDown(VK_MENU))) &&
		((m_shift == 0 && !isVkDown(VK_SHIFT)) || (m_shift == 1 && isVkDown(VK_SHIFT))) &&
		isVkDown(m_vk);
	else
		return
		(m_ctrl == 0 || isVkDown(VK_CONTROL)) &&
		(m_alt == 0 || isVkDown(VK_MENU)) &&
		(m_shift == 0 || isVkDown(VK_SHIFT));
}

void KeyBind::update()
{
	bool keyDown = isKeyDown();

	if (keyDown && !m_status.isDown)
		m_status.state = InpuKeyBindState::WentDown;
	else if (!keyDown && m_status.isDown)
		m_status.state = InpuKeyBindState::WentUp;
	else
		m_status.state = InpuKeyBindState::Idle;

	m_status.isDown = keyDown;
}


bool KeyBind::isDown() const
{
	return m_status.isDown;
}

bool KeyBind::wentDown() const
{
	return m_status.state == InpuKeyBindState::WentDown;
}

bool KeyBind::wentUp() const
{
	return m_status.state == InpuKeyBindState::WentUp;
}

bool KeyBind::isEmpty() const
{
	return (m_vk == 0 && m_ctrl == 0 && m_alt == 0 && m_shift == 0);
}