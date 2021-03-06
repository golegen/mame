// license:BSD-3-Clause
// copyright-holders:David Haywood, Nicola Salmoria
#ifndef MAME_INCLUDES_USGAMES_H
#define MAME_INCLUDES_USGAMES_H

#pragma once

#include "emupal.h"

class usgames_state : public driver_device
{
public:
	usgames_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_gfxdecode(*this, "gfxdecode"),
		m_videoram(*this, "videoram"),
		m_charram(*this, "charram"),
		m_leds(*this, "led%u", 0U)
	{ }

	void usg32(machine_config &config);
	void usg185(machine_config &config);

protected:
	virtual void machine_start() override;
	virtual void video_start() override;

private:
	required_device<cpu_device> m_maincpu;
	required_device<gfxdecode_device> m_gfxdecode;

	required_shared_ptr<uint8_t> m_videoram;
	required_shared_ptr<uint8_t> m_charram;

	output_finder<5> m_leds;

	tilemap_t *m_tilemap;

	DECLARE_WRITE8_MEMBER(rombank_w);
	DECLARE_WRITE8_MEMBER(lamps1_w);
	DECLARE_WRITE8_MEMBER(lamps2_w);
	DECLARE_WRITE8_MEMBER(videoram_w);
	DECLARE_WRITE8_MEMBER(charram_w);

	TILE_GET_INFO_MEMBER(get_tile_info);

	DECLARE_PALETTE_INIT(usgames);

	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	void usg185_map(address_map &map);
	void usgames_map(address_map &map);
};

#endif // MAME_INCLUDES_USGAMES_H
