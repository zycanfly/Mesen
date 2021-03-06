---
title: Event Viewer
weight: 13
chapter: false
---

<div class="imgBox"><div>
	<img src="/images/EventViewer.png" />
	<span>Event Viewer</span>
</div></div>

The Event Viewer allows you to visually check the timing at which various events (register read/writes, NMIs, IRQs, etc.) occur. This can be useful when trying to perform timing-critical mid-frame changes to the PPU, or to verify that PPU updates finish before vertical blank ends, etc.

The colors can be configured and it's also possible to define [breakpoints](/debugging/debugger.html#breakpoint-configuration) as marks to be shown on the Event Viewer. This is done by enabling a breakpoint's `Mark on Event Viewer` option.  This allows the event viewer to be used to display virtually any special event that needs to be tracked.