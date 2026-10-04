// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <openxr/openxr.h>

// Warm charcoal surfaces with muted yellow accents, in linear colour
struct SPanelPalette
{
	static constexpr XrVector3f background { .018f, .016f, .012f };
	static constexpr XrVector3f listBackground { .026f, .023f, .018f };
	static constexpr XrVector3f disabledBackground { .045f, .043f, .038f };
	static constexpr XrVector3f row { .048f, .042f, .029f };
	static constexpr XrVector3f button { .035f, .033f, .027f };
	static constexpr XrVector3f buttonOutline { .24f, .19f, .07f };
	static constexpr XrVector3f disabledOutline { .085f, .085f, .085f };
	static constexpr XrVector3f selected { .08f, .061f, .019f };
	static constexpr XrVector3f hover { .24f, .19f, .05f };
	static constexpr XrVector3f text { .83f, .80f, .70f };
	static constexpr XrVector3f disabledText { .30f, .29f, .25f };
	static constexpr XrVector3f disabledButtonText { .13f, .125f, .11f };
	static constexpr XrVector3f accent { .84f, .67f, .19f };
	static constexpr XrVector3f ray { 1.0f, .86f, .32f };
	static constexpr XrVector3f graphCpu { .62f, .40f, .92f };
	static constexpr XrVector3f graphWait { .20f, .55f, .90f };
	static constexpr XrVector3f graphBudget { .22f, .22f, .22f };
	static constexpr XrVector3f healthy { .20f, .72f, .26f };
	static constexpr XrVector3f unhealthy { .85f, .20f, .16f };
};
