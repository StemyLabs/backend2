/**
 * STEMY transactional emails — v2 rusty analog design system
 * Colors / type match client/css/stemy.css (void, brass, bone, buy red, LCD green)
 */

const FONT_DISPLAY =
  "'Silkscreen','Archivo',Arial,Helvetica,sans-serif";
const FONT_BODY =
  "'Archivo',Arial,Helvetica,sans-serif";
const FONT_MONO =
  "'Space Mono','Courier New',Courier,monospace";

const C = {
  void: "#0b0805",
  card: "#16100a",
  cardBorder: "#5e4826",
  plate: "#7c6a4a",
  brass: "#8c6e3a",
  brassHi: "#c4a05e",
  bone: "#d8cdb4",
  muted: "#b2a384",
  dim: "#7a6d55",
  lcd: "#4bff80",
  lcdBg: "#050f08",
  buy: "#d8202a",
  buyHi: "#f03a40",
  buyDk: "#8c0e14",
  heat: "#ff4a2a",
  warnBg: "rgba(255,74,42,0.1)",
  infoBg: "rgba(196,160,94,0.1)",
  lcdGlow: "rgba(75,255,128,0.08)",
};

const ctaButton = (href, label) => `
  <table role="presentation" border="0" cellpadding="0" cellspacing="0">
    <tr>
      <td align="center" style="border-radius:999px;background:linear-gradient(180deg,#ff5f55 0%,${C.buyHi} 18%,${C.buy} 62%,${C.buyDk} 100%);border:1px solid #4a060a;box-shadow:0 8px 20px rgba(0,0,0,0.45);">
        <a href="${href}" target="_blank" style="display:inline-block;padding:14px 28px;font-family:${FONT_BODY};font-size:15px;font-weight:800;color:#ffffff;text-decoration:none;border-radius:999px;letter-spacing:0.04em;text-shadow:0 1px 2px rgba(0,0,0,0.4);">${label}</a>
      </td>
    </tr>
  </table>
`;

const callout = (text, variant = "info") => {
  const border = variant === "warn" ? C.heat : C.brassHi;
  const bg = variant === "warn" ? C.warnBg : C.infoBg;
  const color = variant === "warn" ? "#ff9e8a" : C.muted;
  return `
  <table role="presentation" border="0" cellpadding="0" cellspacing="0" width="100%" style="margin:16px 0 0 0;">
    <tr>
      <td style="padding:12px 16px;background:${bg};border-radius:8px;border-left:3px solid ${border};">
        <p style="margin:0;color:${color};font-size:13px;line-height:1.5;font-family:${FONT_BODY};">${text}</p>
      </td>
    </tr>
  </table>`;
};

const otpBlock = (otp) => `
  <table role="presentation" border="0" cellpadding="0" cellspacing="0" width="100%">
    <tr>
      <td align="center" style="padding:22px 16px;background:${C.lcdBg};border-radius:10px;border:1px solid rgba(75,255,128,0.22);box-shadow:inset 0 0 24px ${C.lcdGlow};">
        <span style="font-family:${FONT_MONO};font-size:36px;font-weight:700;letter-spacing:0.35em;color:${C.lcd};">${otp}</span>
      </td>
    </tr>
  </table>
`;

const LAYOUT = (content) => `
<!DOCTYPE html>
<html lang="en" xmlns="http://www.w3.org/1999/xhtml">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="color-scheme" content="dark">
  <meta name="supported-color-schemes" content="dark">
  <!--[if mso]>
  <noscript><xml><o:OfficeDocumentSettings><o:PixelsPerInch>96</o:PixelsPerInch></o:OfficeDocumentSettings></xml></noscript>
  <![endif]-->
  <link href="https://fonts.googleapis.com/css2?family=Archivo:wght@500;600;700;800&family=Silkscreen:wght@400;700&family=Space+Mono:wght@400;700&display=swap" rel="stylesheet">
  <style>
    body,table,td,a{-webkit-text-size-adjust:100%;-ms-text-size-adjust:100%;}
    table,td{mso-table-lspace:0pt;mso-table-rspace:0pt;}
    img{-ms-interpolation-mode:bicubic;border:0;height:auto;line-height:100%;outline:none;text-decoration:none;}
    table{border-collapse:collapse!important;}
    body{height:100%!important;margin:0!important;padding:0!important;width:100%!important;background-color:${C.void};}
    a{color:${C.brassHi};}
  </style>
</head>
<body style="margin:0;padding:0;background-color:${C.void};">
  <table role="presentation" border="0" cellpadding="0" cellspacing="0" width="100%" style="background-color:${C.void};">
    <tr>
      <td align="center" style="padding:40px 16px;background:
        radial-gradient(ellipse 120% 80% at 50% -10%, #30200f 0%, transparent 55%),
        ${C.void};">
        <table role="presentation" border="0" cellpadding="0" cellspacing="0" width="100%" style="max-width:520px;background-color:${C.card};border-radius:14px;border:1px solid ${C.cardBorder};overflow:hidden;box-shadow:0 16px 40px rgba(0,0,0,0.55), inset 0 1px 0 rgba(216,205,180,0.08);">
          <tr>
            <td height="3" style="background:linear-gradient(90deg,transparent,${C.brass},${C.brassHi},${C.brass},transparent);height:3px;line-height:0;font-size:0;">&nbsp;</td>
          </tr>
          <tr>
            <td align="center" style="padding:28px 32px 0;">
              <span style="font-family:${FONT_DISPLAY};font-size:22px;font-weight:700;color:${C.brassHi};letter-spacing:0.12em;">STEMY</span><br>
              <span style="font-family:${FONT_MONO};font-size:10px;font-weight:400;color:${C.dim};letter-spacing:0.18em;text-transform:uppercase;margin-top:8px;display:inline-block;">Professional Audio Mastering</span>
            </td>
          </tr>
          <tr>
            <td style="padding:24px 32px 32px;font-family:${FONT_BODY};font-size:15px;line-height:1.65;color:${C.muted};">
              ${content}
            </td>
          </tr>
          <tr>
            <td style="padding:0 32px;">
              <table role="presentation" border="0" cellpadding="0" cellspacing="0" width="100%">
                <tr><td height="1" style="background-color:rgba(94,72,38,0.55);font-size:0;line-height:0;">&nbsp;</td></tr>
              </table>
            </td>
          </tr>
          <tr>
            <td align="center" style="padding:20px 32px 28px;font-family:${FONT_MONO};font-size:11px;line-height:1.6;color:${C.dim};text-transform:uppercase;letter-spacing:0.1em;">
              STEMY Labs &bull; 2026<br>
              <span style="color:${C.brassHi};">Built for artists. Engineered to hit.</span>
            </td>
          </tr>
        </table>
      </td>
    </tr>
  </table>
</body>
</html>
`;

export const welcomeEmail = (firstName) => LAYOUT(`
  <h1 style="font-family:${FONT_BODY};font-size:24px;font-weight:800;line-height:1.25;color:${C.bone};margin:0 0 16px 0;">Welcome to STEMY, ${firstName || "artist"}!</h1>
  <p style="margin:0 0 16px 0;color:${C.muted};">Thanks for joining. Your account is ready &mdash; start mastering your music in the studio.</p>
  <p style="margin:0 0 12px 0;font-family:${FONT_MONO};font-size:11px;letter-spacing:0.12em;text-transform:uppercase;color:${C.brassHi};font-weight:700;">What you can do now</p>
  <table role="presentation" border="0" cellpadding="0" cellspacing="0" style="margin:0 0 20px 0;">
    <tr><td style="padding:5px 0;color:${C.muted};font-size:14px;">&bull; Upload a track and hear it mastered in under 90 seconds</td></tr>
    <tr><td style="padding:5px 0;color:${C.muted};font-size:14px;">&bull; Try genre-specific chains (Pop, Hip-Hop, R&amp;B, Rock, and more)</td></tr>
    <tr><td style="padding:5px 0;color:${C.muted};font-size:14px;">&bull; Start your 7-day free trial &mdash; no credit card needed</td></tr>
  </table>
  ${callout("Don't forget to verify your email with the code we just sent you to unlock all features.")}
  <p style="margin:20px 0 4px 0;color:${C.brassHi};font-size:16px;font-weight:700;">Let's make your music sound incredible.</p>
  <p style="margin:0;font-size:13px;color:${C.dim};font-family:${FONT_MONO};">&mdash; Team STEMY</p>
`);

export const verificationOtpEmail = (otp, isResend = false) => LAYOUT(`
  <p style="font-family:${FONT_MONO};font-size:11px;font-weight:700;color:${C.brassHi};margin:0 0 8px 0;text-transform:uppercase;letter-spacing:0.15em;">${isResend ? "New Verification Code" : "Verify Your Email"}</p>
  <p style="margin:0 0 24px 0;color:${C.muted};font-size:15px;">${isResend ? "Here's your new verification code." : "Welcome to STEMY. Use the code below to verify your email address."}</p>
  ${otpBlock(otp)}
  <p style="margin:20px 0 0 0;color:${C.dim};font-size:13px;font-family:${FONT_MONO};">Expires in <strong style="color:${C.bone};">10 minutes</strong>.</p>
  ${callout("If you didn't create this account, you can safely ignore this email.", "info")}
`);

export const passwordResetOtpEmail = (otp) => LAYOUT(`
  <p style="font-family:${FONT_MONO};font-size:11px;font-weight:700;color:${C.brassHi};margin:0 0 8px 0;text-transform:uppercase;letter-spacing:0.15em;">Password Reset</p>
  <p style="margin:0 0 24px 0;color:${C.muted};font-size:15px;">We received a request to reset your STEMY password. Use the code below to proceed.</p>
  ${otpBlock(otp)}
  <p style="margin:20px 0 0 0;color:${C.dim};font-size:13px;font-family:${FONT_MONO};">Expires in <strong style="color:${C.bone};">10 minutes</strong>.</p>
  ${callout("If you didn't request a password reset, ignore this email. Your account is secure.", "warn")}
`);

export const cancellationScheduledEmail = (firstName, periodEnd, url) => LAYOUT(`
  <h1 style="font-family:${FONT_BODY};font-size:22px;font-weight:800;line-height:1.25;color:${C.heat};margin:0 0 16px 0;">Your trial has been cancelled</h1>
  <p style="margin:0 0 12px 0;color:${C.muted};">Hey ${firstName || "artist"},</p>
  <p style="margin:0 0 16px 0;color:${C.muted};">Your STEMY trial has been cancelled. You will continue to have access until <strong style="color:${C.bone};">${periodEnd}</strong>.</p>
  <p style="margin:0 0 12px 0;color:${C.muted};">After this date, your subscription will end and you&rsquo;ll lose access to mastering. No further charges will be made.</p>
  <p style="margin:0 0 16px 0;font-family:${FONT_MONO};font-size:11px;letter-spacing:0.1em;text-transform:uppercase;color:${C.brassHi};font-weight:700;">Change your mind?</p>
  ${ctaButton(url, "Resubscribe")}
  <p style="margin:24px 0 0 0;font-size:13px;color:${C.dim};font-family:${FONT_MONO};">&mdash; Team STEMY</p>
`);

export const cancellationEmail = (firstName, url) => LAYOUT(`
  <h1 style="font-family:${FONT_BODY};font-size:22px;font-weight:800;line-height:1.25;color:${C.bone};margin:0 0 16px 0;">Your subscription has ended</h1>
  <p style="margin:0 0 12px 0;color:${C.muted};">Hey ${firstName || "artist"},</p>
  <p style="margin:0 0 16px 0;color:${C.muted};">Your STEMY subscription has ended. Your access to mastering has been revoked.</p>
  <p style="margin:0 0 16px 0;font-family:${FONT_MONO};font-size:11px;letter-spacing:0.1em;text-transform:uppercase;color:${C.brassHi};font-weight:700;">Ready to come back?</p>
  ${ctaButton(url, "Resubscribe")}
  <p style="margin:24px 0 0 0;color:${C.muted};">Thanks for giving STEMY a try. If you have any feedback, we&rsquo;d love to hear it.</p>
  <p style="margin:12px 0 0 0;font-size:13px;color:${C.dim};font-family:${FONT_MONO};">&mdash; Team STEMY</p>
`);

export const subscriptionWelcomeEmail = (firstName, plan, trialEndsAt, url) => {
  const hasTrial = Boolean(trialEndsAt);
  const endDate = hasTrial
    ? trialEndsAt.toLocaleDateString("en-US", {
        weekday: "long",
        month: "long",
        day: "numeric",
      })
    : null;
  const planLabel = plan === "PRO" ? "Pro" : "Basic";
  return LAYOUT(`
  <h1 style="font-family:${FONT_BODY};font-size:22px;font-weight:800;line-height:1.25;color:${C.bone};margin:0 0 16px 0;">Welcome to ${planLabel}!</h1>
  <p style="margin:0 0 12px 0;color:${C.muted};">Hey ${firstName || "artist"},</p>
  ${
    hasTrial
      ? `<p style="margin:0 0 16px 0;color:${C.muted};">Your ${planLabel} subscription is active with a free trial. Your first payment will be processed on <strong style="color:${C.bone};">${endDate}</strong>.</p>`
      : `<p style="margin:0 0 16px 0;color:${C.muted};">Your ${planLabel} subscription is now active. You have access to STEMY's studio-powered mastering engine.</p>`
  }
  <p style="margin:0 0 12px 0;font-family:${FONT_MONO};font-size:11px;letter-spacing:0.12em;text-transform:uppercase;color:${C.brassHi};font-weight:700;">Unlocked</p>
  <table role="presentation" border="0" cellpadding="0" cellspacing="0" style="margin:0 0 20px 0;">
    <tr><td style="padding:5px 0;color:${C.muted};font-size:14px;">&bull; Master unlimited tracks</td></tr>
    <tr><td style="padding:5px 0;color:${C.muted};font-size:14px;">&bull; Choose from genre-specific chains</td></tr>
    <tr><td style="padding:5px 0;color:${C.muted};font-size:14px;">&bull; Download high-quality 24-bit WAV files</td></tr>
  </table>
  ${ctaButton(url, "Start Mastering")}
  <p style="margin:24px 0 0 0;font-size:13px;color:${C.dim};font-family:${FONT_MONO};">&mdash; Team STEMY</p>
`);
};

export const masterReadyEmail = (sourceName, downloadUrl, dashboardUrl) => LAYOUT(`
  <h1 style="font-family:${FONT_BODY};font-size:22px;font-weight:800;line-height:1.25;color:${C.bone};margin:0 0 16px 0;">Your master is ready</h1>
  <p style="margin:0 0 20px 0;color:${C.muted};">Your mastered track <strong style="color:${C.brassHi};">${sourceName}</strong> has been processed and is ready to download.</p>
  ${ctaButton(downloadUrl, "Download Master")}
  ${callout(
    `This download link is private and expires in 7 days.<br><br>You can also access all your masters from your <a href="${dashboardUrl}" style="color:${C.brassHi};text-decoration:underline;">STEMY dashboard</a>.`,
  )}
  <p style="margin:24px 0 0 0;font-size:13px;color:${C.dim};font-family:${FONT_MONO};">&mdash; Team STEMY</p>
`);

export const trialEndingEmail = (firstName, trialEndsAt, frontendUrl) => LAYOUT(`
  <h1 style="font-family:${FONT_BODY};font-size:22px;font-weight:800;line-height:1.25;color:${C.heat};margin:0 0 16px 0;">Your trial is ending soon</h1>
  <p style="margin:0 0 12px 0;color:${C.muted};">Hey ${firstName || "artist"},</p>
  <p style="margin:0 0 16px 0;color:${C.muted};">Your 7-day free trial will end on <strong style="color:${C.bone};">${trialEndsAt.toLocaleDateString("en-US", { weekday: "long", month: "long", day: "numeric" })}</strong>.</p>
  <p style="margin:0 0 12px 0;font-family:${FONT_MONO};font-size:11px;letter-spacing:0.12em;text-transform:uppercase;color:${C.brassHi};font-weight:700;">Don't lose access</p>
  <table role="presentation" border="0" cellpadding="0" cellspacing="0" style="margin:0 0 20px 0;">
    <tr><td style="padding:5px 0;color:${C.muted};font-size:14px;">&bull; Unlimited studio-powered mastering</td></tr>
    <tr><td style="padding:5px 0;color:${C.muted};font-size:14px;">&bull; Genre-specific mastering chains</td></tr>
    <tr><td style="padding:5px 0;color:${C.muted};font-size:14px;">&bull; High-quality 24-bit WAV downloads</td></tr>
  </table>
  <p style="margin:0 0 20px 0;color:${C.muted};">Upgrade now to keep mastering without interruption.</p>
  ${ctaButton(frontendUrl, "Upgrade Now")}
  <p style="margin:24px 0 0 0;font-size:13px;color:${C.dim};font-family:${FONT_MONO};">&mdash; Team STEMY</p>
`);
