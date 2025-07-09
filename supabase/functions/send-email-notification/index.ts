// supabase/functions/send-email-notification/index.ts

import { serve } from "https://deno.land/std@0.168.0/http/server.ts";

// Ambil kunci API Resend dari environment variables yang akan kita atur nanti
const RESEND_API_KEY = Deno.env.get("RESEND_API_KEY")!;
const FROM_EMAIL = "jamurmen@resend.dev"; // Alamat email default dari Resend
const TO_EMAIL = "bagus251001@gmail.com"; 

console.log("Fungsi 'send-email-notification' v2 siap menerima permintaan.");

/**
 * Fungsi generik untuk mengirim email menggunakan Resend API.
 * @param subject - Judul email.
 * @param htmlBody - Isi email dalam format HTML.
 */
async function sendEmail(subject: string, htmlBody: string) {
  const res = await fetch("https://api.resend.com/emails", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      "Authorization": `Bearer ${RESEND_API_KEY}`,
    },
    body: JSON.stringify({
      from: `Notifikasi Jamur IoT <${FROM_EMAIL}>`,
      to: [TO_EMAIL],
      subject: subject,
      html: htmlBody,
    }),
  });

  if (!res.ok) {
    const errorBody = await res.text();
    console.error("Gagal mengirim email:", errorBody);
    throw new Error("Gagal mengirim email via Resend.");
  }

  console.log("Email berhasil dikirim!");
  return await res.json();
}

// Handler utama yang akan dipanggil via HTTP
serve(async (req) => {
  try {
    const payload = await req.json();
    const notificationType = payload.type;

    let subject = "";
    let htmlBody = "";

    // Memformat email berdasarkan tipe notifikasi yang diterima
    switch (notificationType) {
      case "firmware_update":
        subject = `🚀 Firmware Baru Tersedia: ${payload.version}`;
        htmlBody = `
          <h1>Update Firmware Baru!</h1>
          <p>Versi baru <strong>${payload.version}</strong> telah berhasil di-deploy.</p>
          <p><strong>Catatan Rilis:</strong></p>
          <pre>${payload.release_notes || "Tidak ada catatan."}</pre>
        `;
        break;

      case "critical_alert":
        subject = `⚠️ Peringatan Kritis - Kumbung Jamur`;
        htmlBody = `
          <h1>Peringatan Kritis!</h1>
          <p>Kondisi kelembapan di bawah ambang batas kritis, memerlukan perhatian segera.</p>
          <ul>
            <li><strong>Pesan:</strong> ${payload.message || "Tidak ada pesan."}</li>
            <li><strong>Kelembapan Terukur:</strong> ${payload.humidity?.toFixed(1) || "-"}%</li>
            <li><strong>Suhu Terukur:</strong> ${payload.temperature?.toFixed(1) || "-"}°C</li>
          </ul>
        `;
        break;
      
      case "warning":
        subject = `🔔 Peringatan - Kumbung Jamur`;
        htmlBody = `
          <h1>Notifikasi Peringatan</h1>
          <p>Kondisi lingkungan mendekati ambang batas.</p>
          <ul>
            <li><strong>Pesan:</strong> ${payload.message || "Tidak ada pesan."}</li>
            <li><strong>Kelembapan Terukur:</strong> ${payload.humidity?.toFixed(1) || "-"}%</li>
            <li><strong>Suhu Terukur:</strong> ${payload.temperature?.toFixed(1) || "-"}°C</li>
          </ul>
        `;
        break;

      case "info":
        subject = `ℹ️ Info - Kumbung Jamur Kembali Normal`;
        htmlBody = `
          <h1>Notifikasi Informasi</h1>
          <ul>
            <li><strong>Pesan:</strong> ${payload.message || "Tidak ada pesan."}</li>
            <li><strong>Kelembapan Saat Ini:</strong> ${payload.humidity?.toFixed(1) || "-"}%</li>
            <li><strong>Suhu Saat Ini:</strong> ${payload.temperature?.toFixed(1) || "-"}°C</li>
          </ul>
        `;
        break;
        
      default:
        throw new Error(`Tipe notifikasi tidak valid: ${notificationType}`);
    }

    await sendEmail(subject, htmlBody);

    return new Response(JSON.stringify({ message: "Notifikasi email berhasil diproses." }), {
      headers: { "Content-Type": "application/json" },
      status: 200,
    });

  } catch (error) {
    console.error("Error di Edge Function:", error.message);
    return new Response(JSON.stringify({ error: error.message }), {
      headers: { "Content-Type": "application/json" },
      status: 500,
    });
  }
});