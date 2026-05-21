use eframe::egui; // GUI
use aes_gcm::{Aes256Gcm, Key, Nonce};
use aes_gcm::aead::{Aead, KeyInit, OsRng};
use aes_gcm::aead::rand_core::RngCore; // Encryption
use serde_json::json;       // Used for saving people (contacts)
use std::time::{SystemTime, UNIX_EPOCH};
use chrono::{DateTime, Utc};
use chrono::Timelike; // Timestamps
use std::sync::{Arc, Mutex};
use rand::RngExt;
/*
 All-inclusive ---^
 If you fork this, mention the author (ffrxst), also can add the link to refer to the original.
 Built by a mechatronics engineering student.
 Also part of the "Project Freedom" subdivision, which specializes in open source decentralized apps which are harder to ban than usual apps.
 Protected by the EUPL-1.2 license.
 Fear no code, the comments are intended to give you the ideas and why is a part of the code here.
 Built with support for dictatorship-affected countries, which restrict internet availability and forbid privacy. Includes encryption, compression, localized data
 and complete decentralization. Own your data.
 For anyone wondering how is this supposed to work, this is intended to be spread via bittorrent networks, also works similarly (peer to peer, node to node.)
 Also, this is also built to work even in case of a nuclear war. (Only if the device isn't fried by radiation, and you know how to make any electronic device
 a router)
 Sorry for if the code is too complicated and obscured. Use Ctrl+F, keywords are usually in the comments near sections.
 STYLE marks "You can edit this to style it as you'd like", basically colors usually.
 */

// The cool greetings, QOL convenience and so on, also tells you the part of the day I guess
fn greeting(name: &str, friendly: bool) -> String {     // Friendly and corporate versions. Friendly for humans, corporate for gingers because they have no soul
    let hour = chrono::Local::now().hour();
    let msg = match (hour, friendly) {
        (5..=8, false)  => "Good Morning,",
        (5..=8, true)   => "Rise and shine,",
        (12..=13, false) => "Good Afternoon,",
        (12..=14, true)  => "Lunch time,",
        (17..=20, false) => "Good Evening,",
        (17..=19, true)  => "Tea & Chatting,",
        (22..=23, _)     => "Late night chats,",
        (0..=4, _)       => "3AM Chats,",
        _                => "Hello,",
    };
    format!("{} {}", msg, name)
}

// Token nomenclature
const NATO: &[&str] = &[    // NATO Phonetic alphabet, convenient. You can't mistake "Yankee" for "Bravo", used in tokens. X-ray for X swapped for Xenon because of the "-" dash
    "Alpha", "Bravo", "Charlie", "Delta", "Echo",           // And for overall reading convenience as it is two words fitted in one.
    "Foxtrot", "Golf", "Hotel", "India", "Juliet",
    "Kilo", "Lima", "Mike", "November", "Oscar",
    "Papa", "Quebec", "Romeo", "Sierra", "Tango",
    "Uniform", "Victor", "Whiskey", "Xenon", "Yankee", "Zulu"
];

// AppView is for focusing, overlays and such stuff, enum so that only one can be chosen, perfect.
#[derive(Default, PartialEq)]
enum AppView {
    #[default]
    Chat,       // Well, the chat.
    Settings,   // settings overlay, just so it closes whenever you point anything out of it
    Sharing,    // QR codes, token and other methods
    Media,      // pfp, media (pics videos and such)
}

fn main() {
    let rt = tokio::runtime::Runtime::new().unwrap();
    let (tx, rx) = std::sync::mpsc::channel::<String>();

    let ctx_holder: Arc<Mutex<Option<egui::Context>>> = Arc::new(Mutex::new(None));
    let ctx_holder_clone = ctx_holder.clone();

    let handle = rt.handle().clone();

    async fn discover_node(target: &str) -> Option<String> {
        let socket = tokio::net::UdpSocket::bind("0.0.0.0:0").await.ok()?;
        socket.send_to(b"LINEA_HELLO", target).await.ok()?;
        let mut buf = [0u8; 1024];
        let (len, _) = socket.recv_from(&mut buf).await.ok()?;
        let response = String::from_utf8_lossy(&buf[..len]);

        if response.starts_with("LINEA_NODE:") {
            let name = response[11..].to_string();
            return Some(name);
        }
        None
    }

    // тест
    rt.block_on(async {
        match discover_node("192.168.1.72:1234").await {
            Some(name) => println!("Found node: {}", name),
            None => println!("No response"),
        }
    });

    /*
    Our Father, Which art in Heaven, hallowed be Thy Name.
    Thy kingdom come, Thy will be done in earth as it is in Heaven.
    Give us this day our daily bread and forgive us our debts,
    as we forgive our debtors. And lead us not into temptation;
    but deliver us from evil, and allow this async to work,
    for thine is the kingdom, and the power, and the glory, forever!
    Amen
    ✝
    */

    rt.spawn(async move {
        // wait for ctx
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

struct SpottedNode {
    ip: String,         // you need the IP to know where to send, especially in LAN / STUN modes
    name: String,       // displaying doohickey
    last_seen: u64,    // kinda same thing as below ---\/
    missed_pings: u8,  // used for guessing whether the node is live or "dead" (gone)
}

#[derive(Default)]
struct Linea {
    input: String,
    my_name: String,        // your name, for discovery (wouldn't be cool if there were 10 "Anonymous" in your network, innit?"
    name_input: String,     // temporary to fix the login bug (skips name after first letter)
    my_surname: String,     // optional.
    messages: Vec<Message>,  // Not string because format
    people: Vec<Contact>,   // people you know, the phone book basically
    show_menu: bool,        // is the menu shown?
    selected_chat: usize,   // what chat did you choose in the people menu
    master_key: [u8; 32],   // needed for encryption, IDs and such stuff
    right_panel: RightPanel,    // the right panel is the account info, like the name pfp last online blahblahblah
    network_rx: Option<std::sync::mpsc::Receiver<String>>,   // receive messages
    rt: Option<tokio::runtime::Handle>,     // send messages
    my_token: Option<String>,   // cool thing for sharing your account
    token_expires: Option<u64>, // Unix timestamp when it expires
    app_view: AppView,          // makes the windows a bool so you can't open at the same time settings, media and such, basically just improvements, especially for
                                // overlay windows
    spotted: Vec<SpottedNode>,  // Spotted node, basically "who am I seeing". Used for meeting people, since you have to know what IP to send the token to and such stuff.
}

fn generate_token() -> String {
    use rand::Rng;
    let mut rng = rand::rng();
    let a = NATO[rng.random_range(0..26)]; // First part of the token. 1 in 26 letters, 1/26 chance to guess.
    let b = NATO[rng.random_range(0..26)]; // Second part of the token.  26 * 25 = 650. 1/650 chance to guess.
    let c = NATO[rng.random_range(0..26)]; // Third part of the token. 1/16250 chance to guess if my maths are right.
    format!("{} {} {}", a, b, c)                                // Supposed to rotate every 5 minutes btw, codes work for 6 minutes (in case desynced)
}

#[derive(serde::Serialize, serde::Deserialize)]
struct Contact {
    name: String,   // the person's name
    pubkey: Vec<u8>,    // public key, encryption thing
    last_seen_ip: String,   // when were they last seen, not for reducing privacy but to know if the person is ignoring you or smth idk, I just added it
    data_port: u16,     // the port that receives the stuff, basically you have an identification port (as a standart) and the data port, maybe the softest defense against spamming possible
    last_online: String,     // "2026-05-16/20:51" - looking time.
    known_since: Option<String>, // Unix timestamp of first message
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


// Timestamp formatting
fn format_timestamp(timestamp: &str) -> String {
    let msg_secs: u64 = timestamp.parse().unwrap_or(0); // This thing just makes timestamps be timestamps, not a bunch of UNIX numbers
    let now_secs = SystemTime::now()    // this is just for more comfort while using, this is probably way easier to understand, innit?
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs();

    let diff = now_secs.saturating_sub(msg_secs);

    match diff {
        0..=30 => "Now".to_string(), // 0-30 seconds counts as now I guess
        31..=119 => "A minute ago".to_string(), // 30 seconds - almost 2 minutes.
        120..=1799 => format!("{} minutes ago", diff / 60), // 2 minutes - almost 30 minutes.
        1800..=3539 => "Half an hour ago".to_string(),  // 30 minutes - 45 (I think) minutes
        3540..=86399 => "An hour ago".to_string(), // 45 - 60 minutes I think. I write comments only after writing the code, don't judge.
        86400..=172799 => {
            // chrono doohickey
            "Yesterday".to_string()
        },
        172800..=1296000 => format!("{} days ago", diff / 86400), // Before half a month ago, after "yesterday".
        1296001..=1382400 => "Half a month ago".to_string(), // 15-16 days if I'm correct.
        1382401..=2678400 => format!("{} days ago", diff / 86400), // how many days ago?
        _ => timestamp.to_string(), // leave old timestamps as they are
    }
}

fn save_contact(contact: &Contact) -> Result<(), Box<dyn std::error::Error>> { // Saves contacts because sometimes the contact might not be written down in the .json
    let path = format!("contacts/{}.json", contact.name);
    let json = serde_json::to_string_pretty(contact)?;
    std::fs::write(path, json)?;
    Ok(())
}

// Message saving
fn save_messages(messages: &Vec<Message>, contact_name: &str, key: &[u8; 32])   // that saves messages with encryption, compression and stuff, it's for chat history
                 -> Result<(), Box<dyn std::error::Error>>                      // because this can work without internet, and we don't have servers. Saved in .bin
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

    Ok(())  // Ok
}

// Message loading
fn load_messages(contact_name: &str, key: &[u8; 32])    // Same as before, but reversed
                 -> Result<Vec<Message>, Box<dyn std::error::Error>>
{
    // 1. reading
    let file_data = std::fs::read(format!("msg/{}.bin", contact_name))?; // Read the file

    // 2. nonce,data...
    let nonce_bytes = &file_data[..12]; // Basically like a key but not. first 12 bytes.
    let encrypted = &file_data[12..];   // The encrypted messages

    // 3. Encryption
    let cipher = Aes256Gcm::new(Key::<Aes256Gcm>::from_slice(key));
    let nonce = Nonce::from_slice(nonce_bytes);
    let compressed = cipher.decrypt(nonce, encrypted)
        .map_err(|e| e.to_string())?;

    // 4. Unpack
    let json = zstd::decode_all(compressed.as_slice())?; // think of it as unzipping a .zip

    // 5. Deserialize
    let messages = serde_json::from_slice(&json)?; // format it back to what it should be
    Ok(messages) // Ok
}

// Known since, not really a "useful" thing, but makes it cool, and you can know how much you know the person for, maybe convenience and QOL i guess
fn format_known_since(timestamp: &str) -> String {
    let secs: i64 = timestamp.parse().unwrap_or(0);
    let dt = DateTime::from_timestamp(secs, 0).unwrap();
    dt.format("Known since %d %B %Y").to_string() // Known since 1 January 2001
}

// Short ID, a shorter version of publickey, as unique and used just for maybe quicker identification in some cases, precaution kinda
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
        visuals.panel_fill = egui::Color32::from_rgb(18, 18, 24); // dark blue bg.
        visuals.window_fill = egui::Color32::from_rgb(18, 18, 24); // STYLE
        cc.egui_ctx.set_visuals(visuals);
        let mut fonts = egui::FontDefinitions::default();

        let mut app = Self::default();
        app.selected_chat = usize::MAX;

        app.my_token = Some(generate_token());
        app.token_expires = Some(
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap()
                .as_secs() + 300
        );

        fonts.font_data.insert(
            "noto_sans".to_owned(),
            egui::FontData::from_static(include_bytes!("/home/ffrxst/RustroverProjects/Linea/fonts/NotoSans-Regular.ttf")).into(), // Hardcoded, notosans because it renders
        );                                                                                                                      // everything.

        fonts.families
            .get_mut(&egui::FontFamily::Proportional)
            .unwrap()
            .insert(0, "noto_sans".to_owned());

        cc.egui_ctx.set_fonts(fonts);
        let mut contacts: Vec<Contact> = Vec::new();
        if let Ok(entries) = std::fs::read_dir("contacts") {        // People, I prefer to call them contacts, don't judge
            for entry in entries.flatten() {
                if let Ok(json) = std::fs::read_to_string(entry.path()) {
                    if let Ok(contact) = serde_json::from_str(&json) {
                        contacts.push(contact);
                    }
                }
            }
        }

        let mut app = Self::default(); // All the inits ---\/

        app.rt = Some(rt);
        app.network_rx = Some(rx);
        app.people = contacts;
        app
    }
    fn reload_contacts(&mut self) { // Reload contacts (people) because sometimes new contacts added don't render, rare but precaution.
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

impl eframe::App for Linea { // GUI stuff ---\/
    fn ui(&mut self, ui: &mut egui::Ui, frame: &mut eframe::Frame) {
        // в самом начале fn ui, до всего остального
        if self.my_name.trim().is_empty() {
            egui::CentralPanel::default().show(ui.ctx(), |ui| {
                ui.centered_and_justified(|ui| {
                    ui.vertical_centered(|ui| {
                        ui.heading("Welcome to Linea");
                        ui.add_space(20.0);
                        ui.label("Enter your name to continue:");
                        ui.text_edit_singleline(&mut self.name_input); // temporary, has to be here
                        ui.add_space(10.0);
                        if ui.button("Continue").clicked() && !self.name_input.trim().is_empty() {
                            self.my_name = self.name_input.clone(); // only via button, to prevent misfires on first letter
                            std::fs::create_dir_all("config").ok();
                            std::fs::write("config/name.txt", &self.my_name).ok();
                        }
                    });
                });
            });
            return;
        }

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

        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_secs();

        if self.token_expires.map_or(true, |exp| now >= exp) {
            self.my_token = Some(generate_token());
            self.token_expires = Some(now + 300);
        }

            egui::SidePanel::left("chats_panel").show(ui.ctx(), |ui| {
                ui.horizontal(|ui| {
                    if ui.button("☰").clicked() {
                        self.show_menu = !self.show_menu;
                    }
                    ui.label(greeting(&self.my_name, true));
                });
                ui.separator();
                ui.add_space(10.0);
                self.reload_contacts();
                let mut update_known_since: Option<usize> = None;

                for (i, person) in self.people.iter().enumerate() {
                    ui.add_space(4.0);
                    let is_selected = self.selected_chat == i;      // Bright when selected, dark when unselected. Simple.
                    let fill = if is_selected {                  // STYLE
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
                        self.app_view = AppView::Chat;
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

        match self.app_view {
            AppView::Chat => {
                // contact panel
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
                // bottom panel
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
                                            egui::Frame::new() // STYLE
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
                            ui.label("Choose a person you'd like to text"); // Because it sometimes causes panic if you send a message to an unselected chat or smth like that
                        });                                                             // And I'm kinda lazy to play ping pong with stupid requests of users
                    });
                }
            }
            AppView::Sharing => {
                egui::CentralPanel::default().show(ui.ctx(), |ui| {
                    ui.heading("Share Account");
                    // token here
                });
            }
            AppView::Settings => {
                egui::CentralPanel::default().show(ui.ctx(), |ui| {
                    ui.heading("Settings");
                    ui.separator();
                    ui.add_space(10.0);

                    ui.label("Your name:");
                    ui.text_edit_singleline(&mut self.my_name);

                    ui.label("Last name (optional):");
                    ui.text_edit_singleline(&mut self.my_surname);

                    if ui.button("Save").clicked() {
                        // сохраняем в файл
                        std::fs::write("config/name.txt", &self.my_name).ok();
                    }
                });
            }
            AppView::Media => {
                egui::CentralPanel::default().show(ui.ctx(), |ui| {
                    ui.heading("Media");
                });
            }
        }

        if self.app_view == AppView::Sharing {
            egui::CentralPanel::default().show(ui.ctx(), |ui| {
                ui.heading("Share Account");
                ui.separator();
                ui.label("Your token:");
                if let Some(token) = &self.my_token {
                    let now = SystemTime::now()
                        .duration_since(UNIX_EPOCH)
                        .unwrap()
                        .as_secs();

                    if let Some(expires) = self.token_expires {
                        if now < expires {
                            ui.heading(token);
                            ui.label(format!("{} seconds left", expires - now));
                        } else {
                            self.my_token = None;
                            self.token_expires = None;
                        }
                    }
                }
                if ui.button("Generate New Token").clicked() {
                    self.my_token = Some(generate_token());
                    self.token_expires = Some(
                        SystemTime::now()
                            .duration_since(UNIX_EPOCH)
                            .unwrap()
                            .as_secs() + 300
                    );
                }
            });
        }

        if self.show_menu {
            egui::Window::new("##menu")
                .fixed_pos([35.0, 35.0]) // under header, to keep it spacious and breathing
                .fixed_size([280.0, ui.ctx().screen_rect().height()])
                .title_bar(false)
                .show(ui.ctx(), |ui| {
                    // pfp + имя
                    ui.horizontal(|ui| {
                        ui.label("👤"); // Just a hardcode too, real one later
                        ui.vertical(|ui| { // STYLE
                            ui.label("ffrxst");
                            ui.label("Online");
                        });
                    });
                    ui.separator();
                    if ui.button("⚙ Settings").clicked() {
                        self.app_view = AppView::Settings;
                        self.show_menu = false;
                    }
                    if ui.button("🔗 Share").clicked() {
                        self.app_view = AppView::Sharing;
                        self.show_menu = false;
                    }
                });
        }
    }
}