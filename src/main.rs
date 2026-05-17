use eframe::egui;
use aes_gcm::{Aes256Gcm, Key, Nonce};
use aes_gcm::aead::{Aead, KeyInit, OsRng};
use aes_gcm::aead::rand_core::RngCore;
use serde_json::json;
use std::time::{SystemTime, UNIX_EPOCH};
use chrono::{DateTime, Utc};
use chrono::Timelike;
use std::sync::{Arc, Mutex};
use rand::RngExt;

fn greeting(name: &str, friendly: bool) -> String {
    let hour = chrono::Local::now().hour();
    let msg = match (hour, friendly) {
        (5..=8, false)  => "Good Morning,",
        (5..=8, true)   => "Rise and shine,",
        (12..=13, false) => "Good Afternoon,",
        (12..=13, true)  => "Lunch time,",
        (17..=18, false) => "Good Evening,",
        (17..=18, true)  => "Tea & Chatting,",
        (22..=23, _)     => "Late night chats,",
        (0..=4, _)       => "3AM Chats,",
        _                => "Hello,",
    };
    format!("{} {}", msg, name)
}

const NATO: &[&str] = &[
    "Alpha", "Bravo", "Charlie", "Delta", "Echo",
    "Foxtrot", "Golf", "Hotel", "India", "Juliet",
    "Kilo", "Lima", "Mike", "November", "Oscar",
    "Papa", "Quebec", "Romeo", "Sierra", "Tango",
    "Uniform", "Victor", "Whiskey", "Xenon", "Yankee", "Zulu"
];

enum AppView {
    Chat,
    ShareAccount,
    Settings,
}

fn main() {
    let rt = tokio::runtime::Runtime::new().unwrap();
    let (tx, rx) = std::sync::mpsc::channel::<String>();

    let ctx_holder: Arc<Mutex<Option<egui::Context>>> = Arc::new(Mutex::new(None));
    let ctx_holder_clone = ctx_holder.clone();

    let handle = rt.handle().clone();

    rt.spawn(async move {
        // ждём пока ctx появится
        loop {
            let ctx = ctx_holder_clone.lock().unwrap().clone();
            if let Some(ctx) = ctx {
                start_listener(1234, tx, ctx).await;
                break;
            }
            tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;
        }
    });

    let native_options = eframe::NativeOptions::default();
    let _ = eframe::run_native("Linea", native_options,
                               Box::new(move |cc| {
                                   *ctx_holder.lock().unwrap() = Some(cc.egui_ctx.clone());
                                   Ok(Box::new(Linea::new(cc, rx, handle)))
                               }));
}


fn random_data_port() -> u16 {
    use rand::Rng;
    rand::rng().random_range(1024u16..=9999u16)
}
async fn start_listener(port: u16, tx: std::sync::mpsc::Sender<String>, ctx: egui::Context) {
    let socket = tokio::net::UdpSocket::bind(format!("0.0.0.0:{}", port))
        .await
        .unwrap();

    let mut buf = [0u8; 65535];
    loop {
        let (len, _addr) = socket.recv_from(&mut buf).await.unwrap();
        let text = String::from_utf8_lossy(&buf[..len]).to_string();
        tx.send(text).ok();
        ctx.request_repaint(); // будим egui
    }
}

async fn send_udp(target: &str, data: &[u8]) {
    if let Ok(socket) = tokio::net::UdpSocket::bind("0.0.0.0:0").await {
        socket.send_to(data, target).await.ok();
    }
}

#[derive(Default)]
struct Linea {
    input: String,
    messages: Vec<Message>,  // Not string because format
    people: Vec<Contact>,
    show_menu: bool,
    selected_chat: usize,
    master_key: [u8; 32],
    right_panel: RightPanel,
    network_rx: Option<std::sync::mpsc::Receiver<String>>,
    rt: Option<tokio::runtime::Handle>,
    my_token: Option<String>,
    token_expires: Option<u64>, // Unix timestamp when it expires
}

fn generate_token() -> String {
    use rand::Rng;
    let mut rng = rand::rng();
    let a = NATO[rng.random_range(0..26)];
    let b = NATO[rng.random_range(0..26)];
    let c = NATO[rng.random_range(0..26)];
    format!("{} {} {}", a, b, c)
}

#[derive(serde::Serialize, serde::Deserialize)]
struct Contact {
    name: String,
    pubkey: Vec<u8>,
    last_seen_ip: String,
    data_port: u16,
    last_online: String,     // "2026-05-16/20:51" - looking time.
    known_since: Option<String>, // Unix timestamp первого сообщения
    pfp_path: Option<String>, // profile pictures, also compressed because pictures fatty
    messages_path: String,    // basically where your chat history goes (dw, encrypted and compressed)
    media_cache_days: u32,  // to prevent your filesystem to go chunky
}

#[derive(serde::Serialize, serde::Deserialize, Clone)]
struct Message {
    sender: String,    // who said that
    text: String,      // what it said
    timestamp: String, // when it said
}

#[derive(Default)]
enum RightPanel {
    ContactCard(usize),  // Guy card
    MemberList,          // GC list
    #[default]
    Hidden,
}



fn format_timestamp(timestamp: &str) -> String {
    let msg_secs: u64 = timestamp.parse().unwrap_or(0);
    let now_secs = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs();

    let diff = now_secs.saturating_sub(msg_secs);

    match diff {
        0..=30 => "Now".to_string(),
        31..=119 => "A minute ago".to_string(),
        120..=1799 => format!("{} minutes ago", diff / 60),
        1800..=3539 => "Half an hour ago".to_string(),
        3540..=86399 => "An hour ago".to_string(),
        86400..=172799 => {
            // вчера — нужно chrono для hh:mm
            "Yesterday".to_string()
        },
        172800..=1296000 => format!("{} days ago", diff / 86400),
        1296001..=1382400 => "Half a month ago".to_string(),
        1382401..=2678400 => format!("{} days ago", diff / 86400),
        _ => timestamp.to_string(), // старый timestamp как есть
    }
}

fn save_contact(contact: &Contact) -> Result<(), Box<dyn std::error::Error>> {
    let path = format!("contacts/{}.json", contact.name);
    let json = serde_json::to_string_pretty(contact)?;
    std::fs::write(path, json)?;
    Ok(())
}

fn save_messages(messages: &Vec<Message>, contact_name: &str, key: &[u8; 32])
                 -> Result<(), Box<dyn std::error::Error>>
{
    // 1. serialize
    let json = serde_json::to_vec(messages)?;

    // 2. compress
    let compressed = zstd::encode_all(json.as_slice(), 3)?;

    // 3. encryption
    let cipher = Aes256Gcm::new(Key::<Aes256Gcm>::from_slice(key));
    let mut nonce_bytes = [0u8; 12];
    OsRng.fill_bytes(&mut nonce_bytes);
    let nonce = Nonce::from_slice(&nonce_bytes);
    let encrypted = cipher.encrypt(nonce, compressed.as_slice())
        .map_err(|e| e.to_string())?;

    // 4. saving
    let path = format!("msg/{}.bin", contact_name);
    let mut file_data = nonce_bytes.to_vec();
    file_data.extend(encrypted);
    std::fs::write(path, file_data)?;

    Ok(())
}

fn load_messages(contact_name: &str, key: &[u8; 32])
                 -> Result<Vec<Message>, Box<dyn std::error::Error>>
{
    // 1. reading
    let file_data = std::fs::read(format!("msg/{}.bin", contact_name))?;

    // 2. nonce,data...
    let nonce_bytes = &file_data[..12];
    let encrypted = &file_data[12..];

    // 3. Encryption
    let cipher = Aes256Gcm::new(Key::<Aes256Gcm>::from_slice(key));
    let nonce = Nonce::from_slice(nonce_bytes);
    let compressed = cipher.decrypt(nonce, encrypted)
        .map_err(|e| e.to_string())?;

    // 4. Unpack
    let json = zstd::decode_all(compressed.as_slice())?;

    // 5. Deserialize
    let messages = serde_json::from_slice(&json)?;
    Ok(messages)
}

fn format_known_since(timestamp: &str) -> String {
    let secs: i64 = timestamp.parse().unwrap_or(0);
    let dt = DateTime::from_timestamp(secs, 0).unwrap();
    dt.format("Known since %d %B %Y").to_string()
}

fn short_id(pubkey: &[u8]) -> String {
    pubkey.iter().take(8)
        .map(|b| format!("{:02x}", b))
        .collect::<Vec<_>>()
        .join("")
}

impl Linea {
    fn new(cc: &eframe::CreationContext<'_>, rx: std::sync::mpsc::Receiver<String>, rt: tokio::runtime::Handle) -> Self {
        std::fs::create_dir_all("contacts").ok();
        std::fs::create_dir_all("media/pfp").ok();
        std::fs::create_dir_all("msg").ok();
        let mut visuals = egui::Visuals::dark();
        visuals.panel_fill = egui::Color32::from_rgb(18, 18, 24); // dark blue bg
        visuals.window_fill = egui::Color32::from_rgb(18, 18, 24);
        cc.egui_ctx.set_visuals(visuals);
        let mut fonts = egui::FontDefinitions::default();

        let mut app = Self::default();
        app.selected_chat = usize::MAX;

        fonts.font_data.insert(
            "noto_sans".to_owned(),
            egui::FontData::from_static(include_bytes!("/home/ffrxst/RustroverProjects/Linea/fonts/NotoSans-Regular.ttf")).into(),
        );

        fonts.families
            .get_mut(&egui::FontFamily::Proportional)
            .unwrap()
            .insert(0, "noto_sans".to_owned());

        cc.egui_ctx.set_fonts(fonts);
        let mut contacts: Vec<Contact> = Vec::new();
        if let Ok(entries) = std::fs::read_dir("contacts") {
            for entry in entries.flatten() {
                if let Ok(json) = std::fs::read_to_string(entry.path()) {
                    if let Ok(contact) = serde_json::from_str(&json) {
                        contacts.push(contact);
                    }
                }
            }
        }

        let test_key: [u8; 32] = [42u8; 32]; // test, delete if I forgot to
        app.master_key = [42u8; 32]; // also a hardcoded test
        let test_msgs = vec![
            Message { sender: "me".to_string(), text: "Привет".to_string(), timestamp: "2026-05-17".to_string() },
            Message { sender: "me".to_string(), text: "Тест".to_string(), timestamp: "2026-05-17".to_string() },
        ];
        save_messages(&test_msgs, "test", &test_key).ok();

        let mut app = Self::default();

        app.rt = Some(rt);
        app.network_rx = Some(rx);
        app.people = contacts;
        app
    }
    fn reload_contacts(&mut self) {
        self.people.clear();
        if let Ok(entries) = std::fs::read_dir("contacts") {
            for entry in entries.flatten() {
                if let Ok(json) = std::fs::read_to_string(entry.path()) {
                    if let Ok(contact) = serde_json::from_str(&json) {
                        self.people.push(contact);
                    }
                }
            }
        }
    }
}

impl eframe::App for Linea {
    fn ui(&mut self, ui: &mut egui::Ui, frame: &mut eframe::Frame) {
        if let Some(rx) = &self.network_rx {
            while let Ok(msg) = rx.try_recv() {
                self.messages.push(Message {
                    sender: "unknown".to_string(),
                    text: msg,
                    timestamp: SystemTime::now()
                        .duration_since(UNIX_EPOCH)
                        .unwrap()
                        .as_secs()
                        .to_string(),
                });
            }
        }
            egui::SidePanel::left("chats_panel").show(ui.ctx(), |ui| {
                ui.horizontal(|ui| {
                    if ui.button("☰").clicked() {
                        self.show_menu = !self.show_menu;
                    }
                    ui.label(greeting("ffrxst", true)); // потом из настроек
                });
                ui.separator();
                ui.add_space(10.0);
                self.reload_contacts();
                let mut update_known_since: Option<usize> = None;

                for (i, person) in self.people.iter().enumerate() {
                    ui.add_space(4.0);
                    let is_selected = self.selected_chat == i;
                    let fill = if is_selected {
                        egui::Color32::from_rgb(70, 60, 130)
                    } else {
                        egui::Color32::from_rgb(40, 37, 80)
                    };

                    let response = egui::Frame::new()
                        .fill(fill)
                        .corner_radius(egui::CornerRadius::same(6))
                        .inner_margin(egui::Margin::symmetric(8, 6))
                        .show(ui, |ui| {
                            ui.set_min_width(ui.available_width());
                            ui.label(&person.name);
                        });

                    if response.response.interact(egui::Sense::click()).clicked() {
                        self.selected_chat = i;
                        if person.known_since.is_none() {
                            update_known_since = Some(i);
                        }
                        let name = person.name.clone(); // используй person а не self.people[i]
                        if let Ok(msgs) = load_messages(&name, &self.master_key) {
                            self.messages = msgs;
                        } else {
                            self.messages = vec![];
                        }
                    }
                    ui.add_space(4.0);
                }
                if let Some(i) = update_known_since {
                    self.people[i].known_since = Some(
                        SystemTime::now()
                            .duration_since(UNIX_EPOCH)
                            .unwrap()
                            .as_secs()
                            .to_string()
                    );
                    save_contact(&self.people[i]).ok();
                }
            });

        if self.show_menu {
            egui::Window::new("##menu")
                .fixed_pos([4.5, 35.0]) // под хедером
                .fixed_size([280.0, ui.ctx().screen_rect().height()])
                .title_bar(false)
                .show(ui.ctx(), |ui| {
                    // pfp + имя
                    ui.horizontal(|ui| {
                        ui.label("👤"); // потом реальный pfp
                        ui.vertical(|ui| {
                            ui.label("ffrxst");
                            ui.label("Online");
                        });
                    });
                    ui.separator();
                    ui.label("⚙ Настройки");
                    ui.label("🔗 Поделиться");
                });
        }

        egui::SidePanel::right("contact_card").show(ui.ctx(), |ui| {
            if self.selected_chat < self.people.len() {
                let contact = &self.people[self.selected_chat];
                // pfp круг сверху
                ui.add_space(20.0);
                ui.heading(&contact.name);
                ui.label(short_id(&contact.pubkey));
                ui.separator();
                ui.label(format!("Last seen: {}",
                                 format_timestamp(&contact.last_online)));
                ui.label(format_known_since(&contact.known_since
                    .as_deref().unwrap_or("0")));
            }
        });
        if self.selected_chat < self.people.len() {
            egui::TopBottomPanel::bottom("input_panel").show(ui.ctx(), |ui| {
                ui.horizontal(|ui| {
                    ui.text_edit_singleline(&mut self.input);
                    let send = ui.button("Отправить").clicked()
                        || ui.input(|i| i.key_pressed(egui::Key::Enter));

                    if send && !self.input.trim().is_empty() {
                        let msg = Message {
                            sender: "me".to_string(),
                            text: self.input.clone(),
                            timestamp: SystemTime::now()
                                .duration_since(UNIX_EPOCH)
                                .unwrap()
                                .as_secs()
                                .to_string(),
                        };
                        self.messages.push(msg.clone());
                        self.input.clear();

                        if self.selected_chat < self.people.len() {
                            let contact = &self.people[self.selected_chat];
                            let target = format!("{}:{}", contact.last_seen_ip, contact.data_port);
                            let data = serde_json::to_vec(&msg).unwrap_or_default();

                            if let Some(rt) = &self.rt {
                                rt.spawn(async move {
                                    send_udp(&target, &data).await;
                                });
                            }
                        }

                        // сохраняем
                        if self.selected_chat < self.people.len() {
                            let name = self.people[self.selected_chat].name.clone();
                            save_messages(&self.messages, &name, &self.master_key).ok();
                        }
                    }
                });
            });
            egui::CentralPanel::default().show(ui.ctx(), |ui| {
                ui.heading("Linea");
                ui.separator();
                ui.add_space(10.0);
                egui::ScrollArea::vertical()
                    .stick_to_bottom(true)
                    .show(ui, |ui| {
                        for msg in &self.messages {
                            let is_me = msg.sender == "me";

                            ui.with_layout(
                                egui::Layout::right_to_left(egui::Align::TOP)
                                    .with_main_wrap(false),
                                |ui| {
                                    egui::Frame::new()
                                        .fill(if is_me {
                                            egui::Color32::from_rgb(70, 60, 130)
                                        } else {
                                            egui::Color32::from_rgb(40, 37, 80)
                                        })
                                        .corner_radius(egui::CornerRadius::same(8))
                                        .inner_margin(egui::Margin::symmetric(10, 8))
                                        .show(ui, |ui| {
                                            ui.set_max_width(300.0);
                                            ui.vertical(|ui| {
                                                ui.label(if is_me { "Me" } else { &msg.sender });
                                                ui.label(&msg.text);
                                                ui.with_layout(egui::Layout::right_to_left(egui::Align::TOP), |ui| {
                                                    ui.label(format_timestamp(&msg.timestamp));
                                                });
                                            });
                                        });
                                }
                            );
                            ui.add_space(4.0);
                        }
                    });
                ui.add_space(10.0);
            });
        } else {
            egui::CentralPanel::default().show(ui.ctx(), |ui| {
                ui.centered_and_justified(|ui| {
                    ui.label("Choose a person you'd like to text");
                });
            });
        }
    }
}