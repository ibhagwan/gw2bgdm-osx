#pragma once
#include "meter/config.h"

class KeyBind
{
public:
	KeyBind()
	{}

	KeyBind(int vk, int ctrl, int shift, int alt)
		:m_vk(vk), m_ctrl(ctrl), m_shift(shift), m_alt(alt)
	{
		update();
	}

	KeyBind(Bind *bind)
		:m_vk(bind->vk), m_ctrl(bind->ctrl), m_shift(bind->shift), m_alt(bind->alt)
	{
		update();
	}

	KeyBind& operator=(Bind *bind) {
		if (!bind) return *this;
		m_vk = bind->vk;
		m_ctrl = bind->ctrl;
		m_shift = bind->shift;
		m_alt = bind->alt;
		update();
		return *this;
	}

	KeyBind& operator=(Bind& bind) {
		return (*this = &bind);
	}

	void update();

	bool isDown() const;
	bool wentDown() const;
	bool wentUp() const;
	bool isEmpty() const;

private:
	enum class InpuKeyBindState
	{
		Idle,
		WentDown,
		WentUp
	};

	struct KeyBindStatus
	{
		bool isDown = false;
		InpuKeyBindState state = InpuKeyBindState::Idle;
	};

	int m_vk;
	int m_ctrl;
	int m_shift;
	int m_alt;
	KeyBindStatus m_status;

	bool isKeyDown() const;
};