// supabase/functions/notify-new-firmware/index.ts

import { serve } from "https://deno.land/std@0.168.0/http/server.ts";
import mqtt from "npm:mqtt@5.7.0"; 

// Ambil kredensial dari Environment Variables di Supabase
const MQTT_BROKER = "mqtts://e21436f97e4c46358cda880324a5a6ba.s2.eu.hivemq.cloud:8883";
const MQTT_USER = Deno.env.get("MQTT_USER_SECRET")!;
const MQTT_PASS = Deno.env.get("MQTT_PASS_SECRET")!;
const NOTIFICATION_TOPIC = "jamur/firmware/new_available";

console.log("Fungsi 'notify-new-firmware' siap menerima permintaan.");

interface WebhookRecord {
  version: string;
  file_url: string;
  release_notes?: string;
}

interface WebhookPayload {
  record: WebhookRecord;
}

serve(async (req: Request): Promise<Response> => {
  try {
    // Ambil data baris baru yang dikirim oleh webhook
    const { record }: WebhookPayload = await req.json();
    console.log("Webhook diterima untuk versi:", record.version);

    // Pastikan data yang dibutuhkan ada
    if (!record.version || !record.file_url) {
      throw new Error("Data 'version' atau 'file_url' tidak ditemukan di payload webhook.");
    }
    
    // Hubungkan ke broker MQTT
    const client: mqtt.MqttClient = mqtt.connect(MQTT_BROKER, {
      username: MQTT_USER,
      password: MQTT_PASS,
      clientId: `supabase-function-${Math.random().toString(16).slice(2)}`,
    });

    // Buat sebuah Promise agar fungsi menunggu koneksi dan publish selesai
    await new Promise<boolean>((resolve, reject) => {
      client.on("connect", () => {
        console.log("Terhubung ke broker MQTT untuk mengirim notifikasi.");

        const payload: string = JSON.stringify({
          version: record.version,
          release_notes: record.release_notes || "Tidak ada catatan rilis.",
          url: record.file_url,
        });

        // Publish pesan dengan flag 'retained' agar frontend selalu mendapat notif terakhir
        client.publish(NOTIFICATION_TOPIC, payload, { qos: 1, retain: true }, (err?: Error) => {
          if (err) {
            console.error("Gagal mempublikasikan pesan:", err);
            client.end();
            reject(err);
          } else {
            console.log(`Notifikasi untuk versi ${record.version} berhasil dipublikasikan!`);
            client.end();
            resolve(true);
          }
        });
      });

      client.on("error", (err: Error) => {
        console.error("Error koneksi MQTT:", err);
        client.end();
        reject(err);
      });
    });

    return new Response(JSON.stringify({ message: "Notifikasi berhasil dikirim!" }), {
      headers: { "Content-Type": "application/json" },
      status: 200,
    });

  } catch (error: any) {
    console.error("Terjadi error di Edge Function:", error.message);
    return new Response(JSON.stringify({ error: error.message }), {
      headers: { "Content-Type": "application/json" },
      status: 500,
    });
  }
});