# No Light After Midnight: Mekanik Sistemleri Kurulumu

## 1. Kod (Claude projeye yerleştirdi)
- Kodlar `Source/NoLightAfterMidnight/Mechanics/` içinde. `Build.cs` dosyasına `SlateCore`, `NavigationSystem` ve `Mechanics` include yolu eklendi.
- `.uproject` dosyasına sağ tıkla, **Generate Visual Studio project files** seç, sonra derle.
5. Project Settings > Input > **Default Input Component Class** = `EnhancedInputComponent` olmalı (UE5'te varsayılan zaten bu).

## 2. Sesleri içe aktar
Sesler `Content/Audio/NL/` klasöründe. Editör içe aktarmayı sorarsa kabul et. Sormazsa Content Browser'da o klasöre sağ tıklayıp Import ile ekle.
Adı `_loop` ile biten dosyaları açıp **Looping** seçeneğini işaretle (generator_hum_loop, monitor_hum_loop, drowned_breath_loop, runner_chase_loop, ambient_wind_sea_loop).

| Ses | Nereye atanır |
|---|---|
| generator_hum_loop / start / stop / breakdown | NLGenerator → Hum Loop / Engine Start / Engine Stop / Breakdown |
| repair_hit / repair_mistake / repair_complete / refuel | NLGenerator → Repair Hit / Mistake / Complete / Refuel |
| fuse_switch_on / fuse_switch_off | NLFuseBox → Switch On / Off (Trip için de off) |
| camera_switch / computer_on / computer_off / monitor_hum_loop | NLSecurityDesk |
| light_flash / flash_fail | NLSecurityCamera → Flash / Flash Fail |
| drowned_appear / drowned_breath_loop / drowned_screech / jumpscare | NLDrowned → Appear / Breath Loop / Screech / Jumpscare |
| runner_scream / runner_chase_loop / kill | NLRunner → Chase Start Scream / Chase Loop / Kill |
| footstep | NLPlayerCharacter → Footstep |
| flashlight_click | Player'daki Flashlight bileşeni → Toggle / Empty |
| fuel_pickup | NLFuelCan → Pickup |
| charger_dock / charge_full | NLFlashlightCharger → Dock, Undock / Full |
| day_start / day_end / game_over / win / ambient_wind_sea_loop | NLGameMode |

> Bu sesler kodla sentezlenmiş geçici (placeholder) seslerdir. Final için gerçek kayıtlarla değiştirin; kodda bir değişiklik gerekmez.

## 3. Sahneyi kur
> **Otomatik kurulum yapıldı.** Sesler artık koddan otomatik atanıyor (alan boşsa `Content/Audio/NL/` içinden yüklenir), döngü seslerinin Looping ayarı açık. Lvl_NLTest'e eksik aktörler `Scripts/nl_setup_scene.py` ile kondu: 8 kamera (yer adı, açı ve Drowned noktasıyla birlikte), fener şarjı, benzin bidonu, Runner, NavMesh, masa ve monitör (M_NLCameraMonitor). Haritayı değiştirdikten sonra tekrar çalıştırmak güvenlidir, var olanları çoğaltmaz. Editörde **Tools > Execute Python Script** ile çalıştırabilirsin.
>
> Aşağıdaki adımlar elle kurulum ya da başka harita içindir.

1. **World Settings > GameMode Override** = `NLGameMode` (Default Pawn otomatik olarak `NLPlayerCharacter` olur).
2. Sahneye şunları yerleştir:
   - `NLGenerator` (benzin göstergeli jeneratör)
   - `NLFuseBox` ve hemen yanına `NLFlashlightCharger`
   - `NLFuelCan` (kulübenin içine)
   - 8 tane `NLSecurityCamera`. Her birinin **Camera Name** değerini Cam01…Cam08 yap. Kırmızı oku (DrownedSpawnPoint) Drowned'ın duracağı yere, spot ışığın menziline taşı.
   - `NLSecurityDesk`. **Cameras** dizisine 8 kamerayı sırayla ekle, **Feed Render Target** alanına bir Render Target ata (ör. RT_CameraFeed, 1280x720).
   - `NLDrowned` (Skeletal Mesh ve Anim Class'ı Details'tan ata) ve `NLDrownedDirector`. Director'da **Desk** ve **Placed Drowned** alanlarını seç.
   - `NLRunner`: konduğu yer, kovalamaya başladığı yer olur. Mesh ve Anim Class'ını ata.
   - **NavMeshBoundsVolume**: Runner'ın koşacağı tüm alanı kaplamalı (P tuşuyla yeşil alan görünür).
3. **Monitör materyali:** `FeedTexture` adlı bir Texture parametresi ve `Power` adlı bir Scalar parametresi olsun. Emissive = FeedTexture × Power. Bu materyali NLSecurityDesk'in MonitorMesh'ine ata.

## 4. Animasyonlar
- Drowned, Runner ve oyuncu gövdesi için Animation Blueprint'lerin **Parent Class**'ı `NLCreatureAnimInstance` olmalı. `Speed` / `bIsMoving` ile Idle↔Walk/Run geçişini yap.
- AnimGraph'ta state machine çıkışını **Slot 'DefaultSlot'** düğümünden geçir. Montajlar ancak böyle oynar.
- Montajları Details panelinden ata:
  - Drowned: Appear, **Cover Face** (elini yüzüne kapatma), Flee (kaçış), Jumpscare
  - Runner: Spawn, Kill
  - Player: Sit, Stand, Death
  - Generator: Player Repair / Refuel; FuseBox: Player Switch; FuelCan: Player Pickup
- Montaj atanmadığında kod Fallback süreleriyle çalışmaya devam eder, oyun bozulmaz.

## 5. HUD
`NLHUDWidget` sınıfından bir Widget Blueprint (WBP_NLHUD) yap. Aşağıdaki isimlerle widget ekle (hepsi opsiyonel):
`DayText, ClockText, InteractPromptText, FlashlightBar, FuelCanText, CameraPanel, NoPowerPanel, CameraNameText, FuelBar, FlashCooldownBar, GeneratorStatusText, RepairPanel, RepairProgressBar, RepairZone, RepairNeedle, CenterMessageText, GameOverPanel`

- `RepairZone` ve `RepairNeedle` Image olsun, sola hizalansın. RepairZone'un genişliği **1 px** olsun (kod onu X ekseninde ölçekleyerek bölgeyi çizer). Bar genişliği varsayılan 400 px (`RepairTrackWidth`).
- WBP'yi **`Content/UI/WBP_NLHUD`** yoluna kaydet. Kod onu otomatik olarak bulur.

### Kamera ekranı (CCTV görünümü)
Ekstra asset gerekmez, hepsi kodla kurulur:
- **Filtre:** Her `NLSecurityCamera`'nın Details > **NL|CCTV** bölümünde gri ton, kontrast, pozlama (karanlıkta görüntüyü aydınlatır), gren, vinyet, bloom ve renk kayması ayarları var. Görüntü çok karanlık ya da çok parlaksa önce **CCTV Exposure Bias** değerini değiştir.
- **Ekran üstü efektler:** karıncalanma, tarama çizgileri, aşağı kayan parlak bant, köşe kararması ve kamera değişince parazit. Yoğunlukları HUD'daki **NL|CCTV** ayarlarından değiştirilir.
- **Yerleşim:** sol üstte `CAM 1 KİLER`, sağ üstte gece ve kamera ızgarası, ortada nişangah (ışık hazırsa parlak, altındaki çizgi cooldown), sol altta pil (jeneratör benzini), sağ altta tarih ve saat.
- Kamera adını değiştirmek için kameranın **Location Label** alanını doldur (ör. `KİLER`). Boş bırakılırsa Camera Name kullanılır.
- Tarih **Feed Start Year/Month/Day** değerinden başlar, her gece bir gün ilerler.

## 6. Oyuncunun ses ve animasyonlarını Blueprint kullanmadan atamak
`NLPlayerCharacter`'ı sahneye sürükle ve Details panelinde **Auto Possess Player = Player 0** yap. Footstep sesini, Sit/Stand/Death montajlarını, mesh'i ve fener seslerini bu aktörün Details panelinden atayabilirsin. Böylece GameMode'un spawn ettiği boş karakter yerine bu karakter kullanılır.

## Kontroller
| Tuş | Ne yapar |
|---|---|
| WASD / Fare | Yürü / bak |
| Shift (basılı) | Koş (stamina harcar) |
| E | Etkileşim (otur, kalk, tamir et, benzin al/doldur, şalter, fener şarjı) |
| SPACE | Masada: aktif kameranın ışığını yakar. Jeneratörde: tamir vuruşu |
| A/D, ←/→, 1-8 | Masada kamera değiştir |
| F | Fener |

## Test komutları
- Oyun sırasında konsolu (`~` tuşu) açıp `NLDebugRunner` yazarsan Runner hemen oyuncuyu kovalamaya başlar.
- `NLDebugFuseIn 5`: şalter 5 sn sonra kendiliğinden atar. `NLDebugTimeScale 20`: oyun zamanı 20 kat hızlanır.
- Runner'ın konumu oyuncuya NavMesh üzerinden bağlı olmalı. Runner koridorun doğu ucunda duruyor. Yeşil NavMesh alanını görmek için editörde **P** tuşuna bas.

## Oyuncu gövdesi ve pozlar
- Oyuncunun gövdesi artık görünüyor (UE mankeni, kafa gizli). Görünmeyen bir "sürücü" mesh yürüme animasyonunu oynatıyor, görünen gövde (`BodyRig`) bu pozu kopyalayıp kodla büküyor. Animasyon asset'i gerekmiyor.
- **Fener** sağ elde: kapalıyken kalça yanında aşağı bakar, açınca göğüs önüne kalkar ve bakılan yöne tutulur. Işık fenerin ucundan çıkar. Şarja takılıyken elde görünmez.
- **Benzin bidonu:** E'ye basınca karakter eğilir, sol eliyle alır ve taşır. Bidon jeneratöre dökülünce kaybolur.
- **Masa:** Karakter sandalyeye yürür, oturur, ellerini masaya koyar ve monitöre bakar. Kamera ekranı oturma bitince açılır. Kalkarken tersini yapar.
- Ayarlar: oyuncunun Details panelinde **NL|Body** (oturma, eğilme süreleri, gövde ofseti) ve BodyRig'in **NL|Body** (göz konumu) bölümleri.
- Pozları kontrol etmek için konsola `NLDebugShots` yaz: fener, eğilme, taşıma ve oturma pozlarının ekran görüntüleri `Saved/Screenshots` klasörüne alınır.
- **El hareketleri:** Şalterde el kolu tutup çeker (kol çekildiği anda döner). Şarj istasyonunda fener istasyonun üstüne bırakılır ya da oradan alınır. Jeneratörde bidon iki elle kaldırılıp eğilerek dökülür (benzin dökülmenin ortasında eklenir). Süreler: NLFuseBox **Switch Action Duration**, NLFlashlightCharger **Hand Action Duration**, NLGenerator **Refuel Action Duration**.
- **Masadan kalkma:** Ellerini masaya bastırıp öne eğilir, doğrulur ve bir adım geri çekilir (**Stand Duration**, **Stand Step Back**).

## Şalter arızası
Şalter gün içinde 2–4 dakikada bir kendiliğinden atar ve tüm elektrik gider. Ekranda "FUSE TRIPPED! RESET THE BREAKER" yazar, oyuncu şaltere gidip E ile kolu kaldırır. Ayarlar NLFuseBox'ta **Random Trips**, **Min/Max Trip Interval** (saniye). Sayaç sadece gün sürerken ve şalter açıkken ilerler, şalter tekrar açılınca baştan başlar.

## Boğulmuş: ışık tutulmazsa
Üst üste **2 kez** ışık tutulmazsa saldırır (NLDrownedDirector > **Misses Before Attack**). İlk kaçırmada çığlık sesiyle uyarır ve başka kameraya geçer. Başarılı bir ışık sayacı sıfırlar.

## Şalter kolu ve ek sesler
- Şalter kolu kutunun önünden çıkar: **açıkken yukarı, kapalıyken aşağı** bakar ve yumuşakça döner. Mafsal yeri ve açılar NLFuseBox'ta **Lever Pivot / Lever On Angle / Lever Off Angle**.
- Olaylara özel sesler `Scripts/nl_make_sounds.py` ile üretildi: `flashlight_on/off/empty`, `fuse_trip`, `charger_undock`, `drowned_warning/leave/return`, `runner_retreat`, `chair_sit/stand`, `player_death`. Gerçek kayıtlarla aynı isimle değiştirilebilir.
- `NLDebugShots2`: şalter kolu (açık / attı / tekrar açık) ve yürürken fener tutan elin ekran görüntülerini alır.

## Boğulmuş pozları
- Montaj atanmamışsa pozlar kodla verilir: ışık yiyince **iki eliyle yüzünü siper edip geriye eğilir ve titrer**, sonra **arkasını dönüp uzaklaşır**, jumpscare'de **kollarını iki yana açıp kameranın iki yanını kavrar, yüzünü öne atar; tutarken eller titrer ve oyuncunun bakışı sarsılır**. Gerçek montaj atarsan (Cover Face / Flee / Jumpscare) o oynar.
- Ayarlar: NLDrowned > **Flinch Lean Back**, **Flee Distance**, **Fallback Cover Face / Flee Duration**, **Jumpscare Lunge** (yüzün ne kadar yaklaştığı), **Jumpscare Camera Shake** (bakış sarsıntısı).
- Kaybolunca haritanın altına taşınır; böylece ışıkta gölgesi geride kalmaz.
- `NLDebugShots3`: güvenlik kamerasının gözünden belirme, yüz siperi, kaçış, kaybolma ve jumpscare görüntülerini alır.

## Runner jumpscare
- Runner yakalayınca Boğulmuş ile **aynı yakalama jumpscare'i** oynar: kameranın önüne atılır, kollarını iki yana açıp kameranın iki yanını kavrar, yüzünü öne atar, bakış sarsılır. Ortak kod: `NLJumpscareGrab`.
- Yakalanma anında oyuncu yere ya da tavana bakıyorsa bakış yataya düzeltilir, yüz hep tam karşıda olur (Boğulmuş için de geçerli).
- Runner'ın görünen gövdesi de artık `BodyRig`: koşarken kolları serbestçe sallanır. Gizlenince haritanın altına taşınır (gölge kalmaz).
- Ayarlar: NLRunner > **Jumpscare Distance / Lunge / Camera Shake**. Kill Montage atarsan onun yerine o oynar.
- `NLDebugShots4`: Runner kovalaması ve yakalama görüntüleri.

## Sunum: günü kısaltma
- **F9** (oyun sırasında): bu gün 5 sn sonra biter, "DAY 2" geçişi gösterilir. Sadece geliştirme sürümünde çalışır, paketlenmiş oyunda kapalıdır.
- Konsol: `NLDebugDayLength 5` → bundan sonra her gün 5 sn sürer (saat kaldığı yerden devam eder). `NLDebugDayLength 600` ile normale döner.
- Konsol: `NLDebugEndDayIn 5` → sadece bu gün 5 sn sonra biter.
- Oyunu kısa günlerle başlatmak: başlatma satırına `-NLDayLength=5` ekle (Editor Preferences > Play > Additional Launch Parameters ya da kısayolun hedefine).

## Arıza çakışması
Jeneratör arızası ve şalter atması aynı anda olmaz. Biri arızalıyken ya da düzeltildikten sonraki 45 sn içinde diğeri gelmez; sırası gelen arıza bu süre dolana kadar ertelenir (+5–20 sn rastgele). Süre `UNLPowerSubsystem::MinFailureGap`.
Test: `NLDebugFuseIn 4` + `NLDebugGeneratorIn 6` → şalter atar, jeneratör arızası ertelenir.

## Etkileşim işaretleri
- Etkileşimli her nesnenin (masa, jeneratör, şalter, şarj, bidon) üstünde **7 m**'den itibaren yarı saydam koyu bir yuvarlak belirir; yaklaştıkça netleşir.
- Nesneye bakınca (E menzilinde) yuvarlak büyür, içinde **E** ve yanında eylem yazısı çıkar ("Switch power off" gibi). Yan yana nesnelerde sadece bakılanda E olur, diğerleri yuvarlak kalır.
- Bilgi yazılarında ("Generator - Fuel: 50%") E çıkmaz. Duvar arkasındaki, alınmış ya da kullanılamayan nesnelerde işaret görünmez. Masada, tamirde, eğilirken gizlenir.
- Ayarlar: HUD > **NL|Interact** (Marker Show Distance, Marker Size / Focused Size, Marker Height Offset, Marker Fill).
- `NLDebugShots5`: uzak/yakın işaretlerin arayüzlü ekran görüntülerini alır.

## Runner: tamir hatası
- Tamirde hata yapılınca Runner **uzakta** (oyuncudan 12–26 m, tercihen görmediği bir yerde) belirir ve oradan bir çığlık duyulur. Oyuncu **tamirden çıkana kadar bekler**, çıkınca koşmaya başlar.
- Koşu hızı **250** (oyuncu 280): kaçılabilir, ama durursan ya da sıkışırsan yakalar. Ayarlar: NLRunner > **Chase Speed**, **Spawn Min / Max Distance**.
- Kovalama **süresiz**: Runner ancak oyuncu bir odaya girip kapıyı kapatınca (aşağıda "Saklanma kapısı") ya da oyuncuyu yakalayınca durur. İstersen NLRunner > **Chase Duration** ile süre koyabilirsin (0 = süresiz).
- `NLDebugRunnerRepair`: tamir hatası senaryosunu baştan sona dener (log: `NLTest`).

## Sabotajcı
- Şalterin atması ve jeneratörün bozulması artık görünür bir karakterle oluyor: **Sabotajcı** oyuncunun görmediği bir yerde (hedefe 9–22 m) belirir, **çömelerek sinsice** hedefe yürür, hedefi elleriyle **kurcalar** (alçak hedefte eğilir) ve arıza o sırada olur; sonra **koşarak** görüş dışına kaçıp kaybolur.
- Sahneye konması gerekmez, ilk ihtiyaçta kendiliğinden oluşur. Koyu renkli manken (MI_Manny_02_New).
- O iş başındayken diğer arıza başlamaz; arıza çakışma kuralı (45 sn) bittiği andan itibaren sayılır.
- Hedefe 30 sn içinde ulaşamazsa arızayı yine uygular ve kaybolur (oyun akışı bozulmasın). Yol bulamazsa arıza eskisi gibi hemen olur.
- Ayarlar (NLSaboteur sınıfı varsayılanları): **Sneak Speed** 150, **Flee Speed** 520, **Tamper Duration** 2.6 sn, **Tamper Reach**, **Spawn Min/Max Distance**, **Sneak Crouch Depth**, **Give Up Time**.
- Test: `NLDebugFuseIn 1` / `NLDebugGeneratorIn 1` (Sabotajcı gelir), `NLDebugShots6` (yürüme, kurcalama, kaçış görüntüleri).

## Sigorta kutusu
- Şalter kutusunun paneli sola uzadı: üzerinde 2×2 dizili **4 dikey cam sigorta** (kırmızı, mavi, yeşil, sarı). Yuvaların arkasındaki plakalar artık **renksiz**; hangi rengin nereye gittiği kutuda yazmaz (aşağıda "Teşhis sadece masada").
- **Elektrik** ancak şalter açık **ve** 4 sigorta da doğru yuvadaysa gelir.
- **Sabotajcı** artık şalteri indirmez, sigortaların yerini karıştırır; şalteri bazen indirir, bazen açık bırakır (**Sabotage Switch Off Chance**, varsayılan %50).
- **E** → görüş kutunun **yakın plan kamerasına** kayar (kutudaki loş lamba yanar), fare imleci çıkar. Ayrı bir panel yok: **kutunun üstündeki sigortaya basılı tutup** panelin üstünde sürükle, başka yuvaya bırak → ikisi yer değiştirir (boşa bırakınca yerine döner; sadece tıklamak bir şey yapmaz, renkler doğru olsa da istediğin gibi değiştirebilirsin). **Şalter koluna basılı tutup** fareyi yukarı/aşağı çek: yarıyı geçip bırakınca o tarafa geçer, geçmezse geri döner. Altta sadece kısa bir ipucu satırı var. **E / Esc** ile geri dönülür.
- **Şalter açıkken sigortaya dokunulursa elektrik çarpar:** kamera hızla oyuncuya döner, ekran mavi-beyaz elektrikle cızırdar ve sallanır (oyuncu > **Shock Duration**, **Shock Shake**).
- Doğru sıra: şalteri kapat → sigortaları **monitörde yazan sıraya** yerleştir → şalteri aç. Sigortalar yanlışken şalter kaldırılırsa kıvılcım sesiyle kol geri düşer (**Wrong Wiring Kickback**).
- `NLDebugFuseUI`: sabotaj, elektrik çarpması ve düzeltmenin ekran görüntülerini alır.
- `NLDebugFuseMouse`: gerçek fare girişiyle (imleç + sol tuş) sigorta sürükleme, tek tıklama ve şalter kolunu dener.

## Artı ve etkileşim odağı
- Ekranın ortasında küçük bir **artı** var; etkileşimli bir nesneye bakınca büyür ve parlar. Masada, sigorta kutusunda, tamirde gizlenir.
- **Odak daha rahat:** Nesnenin görünen sınırına **2,8 m**'den yakınsan ve artı nesnenin üstündeyse ya da ona yakınsa (küçük nesnelerde ~7° pay) E çıkar; dibine girince de çalışır. Arada duvar varsa seçilmez. Birden fazla aday varsa artıya en yakın olan seçilir.
- Bakılan nesnenin yuvarlağı ekran dışına düşerse (ör. dibindeyken) **E artının hemen üstünde** görünür.
- Ayarlar: oyuncu > **Interact Distance** (280), **Interact Aim Tolerance** (7°). Test: `NLDebugFocusShots`.

## Saklanma kapısı (Runner'dan kaçış)
- Alt kattaki depo odasının girişine (eski BP_DoorFrame yerine) **NLHideDoor_Storage** kondu (`nl_setup_scene.py`). Kapının yanı ve üstü dolgu duvarla kapanır, NavMesh kapıdan geçer.
- **E** ile kapı açılır/kapanır. Runner kovalarken odaya girip kapıyı **içeriden kapatırsan**:
  - Oyuncu **yüzü kapıya dönük**, kapının dibinde ve kolun hizasında durur, **tüm gövdesiyle kapıya yüklenir** (öne eğik). Kola yakın el kolu tutar, öbür el omuz hizasında kapıyı iter. Kamera kapıya düz, hafif aşağı bakar (koldaki el kadrajın altında). Runner zorladıkça gövde geri itilip tekrar yaslanır; **hızlı panik nefesi** (ses + kamera sallanması) başlar. Yürüme ve bakış kilitlenir.
  - Runner kapının dışına geçer ve **4 kez kapıyı zorlar**: her zorlamada darbe sesi, **kol aşağı basılıp titrer**, kapı aralanıp çarpar, kamera sarsılır ve ekranda **"HOLD THE DOOR!"** çubuğu çıkar. İbre bir kez soldan sağa geçer; **bölgedeyken SPACE**.
  - **Hepsi başarılı:** Runner bırakıp gider (uzaklaşan adımlar), nefes yavaşça söner, oyuncu serbest kalır.
  - **Bölge dışında basarsan ya da geç kalırsan:** kapı içeri savrulur, oyuncu geri itilir ve Runner **jumpscare** ile yakalar (karanlıkta yüzü alttan soluk bir ışıkla görünür).
- Ayarlar: NLHideDoor > **Num Checks** (4), **Settle Time**, **Gap Min / Max**, **Sweep Time** (ibre süresi, her kontrolde **Sweep Speed Up** kadar hızlanır), **Zone Width Start / Min**, **Burst Knockback**; ölçüler **Opening Width / Height**, **Wall Thickness**, **Door Width / Height**. Aktörün ileri oku (+X) odanın **içine** bakmalı.
- Oyuncu > **Breath Shake** (sallanma, derece), **Breath Rate** (nefes/sn), **Hold Look Above Grip** (bakış kolun kaç cm üstüne; küçüldükçe daha aşağı), **Hold Lean** (öne eğilme, derece).
- Yeni sesler (`nl_make_sounds.py`): door_open, door_close, door_bang, door_burst, door_handle, breath_panic_loop, breath_exhausted_loop.
- Test: `NLDebugDoor 1` (kontroller başarılı → Runner gider), `NLDebugDoor 0` (başarısız → kapı kırılır, jumpscare).

## Koşma ve stamina
- **Shift** basılıyken koşulur: hız **480** (yürüme 280, Runner 250). Tam stamina ile ~**5,5 sn** koşulur.
- Bırakınca ~0,9 sn sonra dolmaya başlar (dururken daha hızlı). Stamina **biterse** %30'a dolana kadar koşamazsın. Stamina azaldıkça nefes nefese kalma sesi duyulur.
- **Stamina bitince:** derin, hırıltılı nefes sesi başlar (`breath_exhausted_loop`) ve ekran her nefeste **gri → normal → gri** atar (içe çekişte renk çekilir ve hafif kararır, verişte geri gelir). %30'a dolup tekrar koşabilir hale gelince yavaşça normale döner. Ayarlar: oyuncu > **Exhausted Breath Loop**, **Exhausted Breath Period** (1,1 sn, sesle aynı), **Exhausted Saturation** (0,08).
- **Sıfırlama cezası:** stamina **tamamen biterse** bar **soluk gri** olur ve stamina **tamamen dolana kadar** daha yavaş dolar (normal hızın %45'i; oyuncu > **Depleted Regen Multiplier**). Bitmeden koşmayı bırakırsan normal hızda dolar. %30'dan sonra yine koşabilirsin ama yavaş dolma tam dolana kadar sürer.
- **Gösterge (alt orta):** ince, yıpranmış (çizik, düzensiz kenarlı) bir bar; stamina azaldıkça **iki yandan ortaya doğru kapanır**. Doluyken soluk kemik rengi, azaldıkça **griye** döner (kırmızı değil, can gibi durmasın) ve arızalı ışık gibi titrer. Stamina azaldıkça **ekran kenarları kararır** (tünel görüşü) ve nabız gibi atar; tükenince bar söner-yanar ve titrer. Dolu ve koşmuyorsan kaybolur. Tamamen kodla çiziliyor (`NLHUDStamina.cpp`), asset gerekmez.
- Ayarlar: oyuncu > **Sprint Speed**, **Sprint Duration**, **Stamina Regen Time / Delay**, **Exhausted Recover**, **Panting Loop**.
- Test: `NLDebugStamina` (Shift ile koşu + %75 / %35 / tükenmiş gösterge görüntüleri).

## Ada level'i (Lvl_Ada)
- Esat'ın blockout'u (`Esat` branch'i: `Content/AdaBlockout_YeniV2.umap` + `Ada_Varliklari`, `AdaV2_Varliklari`) **olduğu gibi** alındı, ona dokunulmadı. Oynanış kopyada: **`Content/Levels/Lvl_Ada.umap`**.
- Kurulum scriptle yapılır: `Scripts/nl_setup_ada.py` (tekrar çalıştırmak güvenli). Esat blockout'u güncellerse: yeni dosyaları al, `NL_ADA_RECOPY=1` ile scripti çalıştır, kopya baştan kurulur.
  ```
  UnrealEditor-Cmd.exe NoLightAfterMidnight.uproject -run=pythonscript -script="<tam yol>\Scripts\nl_setup_ada.py" -unattended
  ```
- Yerleşim:
  - **Kontrol binası** (fenerin batısındaki oda): güvenlik masası + monitör (güney duvarı), fener şarjı (batı duvarı), tavan lambası.
  - **Oyuncu** fenerin zemin katında başlar (blockout'taki PlayerStart), zemin katta da bir lamba var.
  - **8 kamera**: blockout'taki `KAMERA_xx` küreleri ve `YON_xx` yön kutuları yerine gerçek kameralar kondu (direkler kaldı). Yön kutusunun gösterdiği yöne bakarlar; Drowned kameranın 6,5 m önünde belirir. İsimler: DOCK, FUEL DEPOT, RUINED HOUSE, WEST PATH, CROSSROADS, NORTH WOODS, EAST PATH, LIGHTHOUSE GATE.
  - **Jeneratör**: haritadaki **`JENERATOR_ANA` binasının kendisi** (ayrı kutu yok): kurulum scripti binanın mesh ve malzemelerini `NLGenerator`a aktarır, blockout aktörünü kaldırır. Benzin, güney duvarda kapının sağındaki **benzin ağzına** dökülür (NLGenerator > **Fuel Inlet Offset**, binanın yerel koordinatı). Test: `NLDebugRefuel`.
  - **Rıhtım kulübesi**: **sigorta kutusu** (arka duvar) ve **benzin bidonu** (kapının yanında, yerde) burada; kapısı aynı zamanda saklanma kapısı. Sigorta atınca ya da benzin gerekince oyuncu kontrol odasından kulübeye kadar yürümek zorunda.
  - **Runner** yıkık evin içinde bekler. **Saklanma kapısı**: rıhtım kulübesinin kapısı (`NLHideDoor_Dock`).
  - **Runner pusu (rastgele olay)**: `NLRunnerAmbush_Shack`, kulübenin çevresinde 14 m yarıçaplı görünmez alan. Oyuncu alana her girişinde zar atılır: 1. gün **%20**, her gün +%7, en fazla %55; günde en fazla 1 pusu. Pusu olursa uzaktan **çığlık** gelir (her yerden duyulur), Runner 16-28 m uzakta belirir ve **3 sn sonra** kovalar. Oyuncu kulübeye girip kapıyı kapatırsa kapı tutma (SPACE kontrolleri) başlar: başarırsa Runner gider, başaramazsa jumpscare. Jeneratör tamir hatasındaki kovalama aynen duruyor. Ayarlar: pusu aktörü > **Radius**, **Chance Day 1**, **Chance Increase Per Day**, **Max Chance**, **Max Per Day**, **Scream To Chase Delay**, **Spawn Min/Max Distance**.
  - **Gece**: ay ışığı, gökyüzü ışığı, sis ve post process test level'den kopyalandı; bulutlar oyunda gizli.
  - **NavMesh** bütün adayı kaplar (alt sınırı deniz seviyesinin üstünde: Runner denizden çıkmaz). Büyük açık alanda Dynamic NavMesh kendiliğinden örülmediği için **oyun başında bir kez tam kurulur** (GameMode > **Build Nav Mesh On Start**, ~0,1 sn).
- Test komutları: `NLDebugAda` (başlangıç, masaya oturma, 8 kamera), `NLDebugChase 54 -400` (oyuncu kavşakta, Runner kovalar, mesafe loglanır), `NLDebugDoor 1/0` (rıhtım kulübesinde kapı tutma), `NLDebugAmbush` (kulübe yakınında kesin pusu: çığlık, kovalama, kulübeye kapanıp kapıyı tutma), `NLDebugTour` (`Saved/tour.txt` içindeki noktalardan ekran görüntüsü).

## Drowned: koşarak gelir, el feneriyle kovalanır
- Drowned artık kameranın önüne **ışınlanmaz**: her kameranın sabit bir **hattı** var. Drowned, kameranın baktığı noktanın 15 m gerisinden (kameradan uzakta) NavMesh üzerinde **koşarak gelir**, varınca kameraya döner. **Tepki süresi varınca başlar.** Masadan bakarken de kameraya doğru koşarak geldiği görülür (ör. DOCK'ta iskelenin deniz ucundan gelir). 8 kameranın hepsinde hat bulunuyor; bulunamazsa eskisi gibi ışınlanır.
- Işık yiyince ya da ışık tutulmayınca **geldiği hattan geri koşar** ve hattın sonunda kaybolur (en fazla 8 sn).
- **El feneri:** oyuncu dışarıdayken el fenerini Drowned'a **0,35 sn** tutarsa (18 m menzil, arada engel yoksa) masadan ışık tutmakla aynı sayılır: Drowned oyuncuya döner, yüzünü kapatır, kaçar. Geliyorken (koşarken) de tutulabilir.
- **Dışarıdayken jumpscare yok:** ışık tutulmadan süre dolarsa kaçırma **sadece oyuncu masada oturuyorsa** sayılır. Oyuncu dışarıdaysa (benzin, sigorta, tamir, gezinme) Drowned hattından geri koşup başka kameraya geçer, sayaç artmaz; dışarıda onu sadece el feneriyle kovalayabilirsin. Test: `NLDebugTimeScale 3` + `NLDebugDrownedAway` (2 dk dışarıda, ölmemeli).
- **Kaos kuralı (aynı anda iki tehdit yok):** Koşucu kovalarken (bağırıp gelme dahil) ya da elektrik arızalıyken (jeneratör bozuk, şalter atmış, yakıt bitmiş) Drowned **gelmez**. O sırada kameraya geliyorsa ya da kameradaysa hattından geri koşup çekilir (kaçırma sayılmaz). Her şey düzelince **12 sn** sakin geçtikten sonra tekrar gelebilir. Ayarlar: Drowned Director > **Hold During Chaos**, **Calm Delay After Chaos**. Test: `NLDebugChaos`.
- Ayarlar: Drowned > **Approach Distance** (1500), **Run Speed** (380), **Run Off Max Time**; Drowned Director > **Flashlight Repel Range**, **Flashlight Repel Time**, **Max Arrive Time** (koşu bu süreyi aşarsa ışınlanır).
- Test: `NLDebugDrowned 4` (oyuncu dışarıda 5. kameranın yanında: Drowned koşarak gelir, el feneriyle kovalanır, geri koşup kaybolur), `NLDebugDrownedRoutes` (8 kameranın hattı var mı).

## Atmosfer (Lvl_Ada)
- Hedef: gerçek karanlık, sis ve el fenerinin öne çıktığı korku havası. Ayarlar `Scripts/nl_setup_ada.py` içindeki **`ATM`** sözlüğünde; değiştirip scripti tekrar çalıştır.
  - **Sabit pozlama** (göz karanlığa alışmaz): `exposure_bias` 0,2. Büyüdükçe her yer aydınlanır.
  - **Soluk ay** (0,3 lux, soğuk mavi-gri) ve gökyüzünü aydınlatmıyor (`atmosphere_sun_light` kapalı); gökyüzü ışığı 0,12.
  - **Hacimli sis** (yoğunluk 0,12): uzaklar sisin içinde kaybolur, **el fenerinin huzmesi görünür** (fenerde Volumetric Scattering 2,5, gölge açık).
  - **Görüntü:** film greni 0,35, vinyet 0,65, hafif renk kayması 0,7, bloom 0,5; renk doygunluğu 0,72, kontrast 1,1, hafif soğuk ton.
  - **İç mekân lambaları** (kontrol odası, fenerin zemin katı): sıcak, loş (30 cd).
- **Güvenlik kameraları** kendi otomatik pozlamasını yapar (gece görüşü): dünya ne kadar karanlık olursa olsun kamera görüntüsü seçilir.
- Test: `NLDebugTour` (`Saved/tour.txt`; 7. sütun `F` = o durakta el feneri açık).

## Ses atmosferi
- GameMode her level'de bir **`NLAmbienceDirector`** oluşturur (level'e elle koymak gerekmez; GameMode > **Spawn Ambience**).
- **Rüzgâr:** eski rüzgâr+deniz döngüsünün üstüne sert rüzgâr katmanı (`amb_wind_gust_loop`); şiddeti yavaşça kabarıp söner.
- **Kıyı dalgaları:** adanın ortasından 12 yöne ışın atılıp kıyı bulunur, oraya **konumlu (3B)** dalga sesleri konur (`amb_waves_loop`). Kıyıya yaklaştıkça duyulur.
- **Uzaktan sesler:** 18–45 sn'de bir, oyuncunun **arkasından ya da yanından**, 15–40 m uzaktan: dal kırılması, metal çınlaması, ayak sesleri, fısıltı, inleme, ağaç gıcırtısı, uzak sis düdüğü, ahşaba vuruş (`dist_*`). Uzaklaştıkça kısılır ve boğuklaşır. **Kovalama, kapı tutma ve ölüm anında susar.**
- **İç mekân:** oyuncunun başının üstünde tavan varsa içeride sayılır: dış sesler **%25'e kısılır ve iyice boğuklaşır** (400 Hz üstü kesilir), alçak bir oda uğultusu (`amb_interior_hum_loop`) gelir.
- **Deniz fenerinin içi:** camlara bastıran boğuk rüzgâr, aralıklardan ince ıslık ve camda tıkırtılar (`amb_glass_wind_loop`); 3–9 sn'de bir **cama çarpan sert rüzgâr** (`glass_gust_hit`): boğuk bir darbe, ardından camın ve çerçevenin titreyip tıkırdaması. Darbe fenerin duvarında rastgele bir camdan, konumlu gelir. `NLWindowGlass` etiketli aktörün (kurulum scripti `DENIZ_FENERI`'ye ekler) içindeyken çalar; başka camlı binalara da bu etiket eklenebilir. Ayarlar: **Glass Wind Volume**, **Glass Hit Volume**, **Glass Hit Gap Min/Max**.
- Ayarlar: NLAmbienceDirector > **Gust/Waves/Interior Volume**, **Distant Gap Min/Max**, **Distant Min/Max Distance**, **Distant Volume**, **Indoor Outside Volume**, **Indoor Low Pass Hz**; uzaktan ses listesi **Distant Sounds**.
- Sesler `nl_make_sounds.py` ile sentezlendi (geçici). Sadece bazılarını yeniden üretmek için: ortam değişkeni `NL_SOUNDS=ad1,ad2`. Gerçek kayıtlarla aynı isimle değiştirilebilir.
- Test: `NLDebugAmbience` (içeride 3, dışarıda 5 uzak ses; içeride/dışarıda geçişi loglanır). Not: `-nosound` ile çalıştırınca ses bileşenleri oluşmaz.

## Ana menü, kayıt/devam, ayarlar, duraklatma
- **Proje açılış level'i:** `Lvl_MainMenu` (Project Settings > Maps & Modes: Game Default Map ve Editor Startup Map). Oyun buradan başlar.
- **Ana menü sahnesi (`Lvl_MainMenu`, `Scripts/nl_make_menu.py`):** gece, denizin ortasında kayalık bir adacık; üstünde deniz feneri, yanında penceresi yanan bekçi kulübesi ve çamlar. Kamera denizde, hafifçe sallanır. **Deniz fenerinin huzmesi fare imlecini izler**: imleci NEW GAME üstüne getirince ışık oraya döner ve yazı aydınlanır (yazılar karanlıkta, ışık vurunca parlar). Yağmur, sis, şimşek, gök gürültüsü, dalga ve rüzgâr sesi. (`ANLMenuDirector`: `Hand Flashlight` kapalı, `Indoor` kapalı; `ANLLighthouse::AimAt`; `ANLMenuGameMode`)
  - **NEW GAME:** `Lvl_Ada_Art` açılır, rıhtım sinematiği ile 1. gece başlar.
  - **CONTINUE:** kayıt varsa son başlanan geceden açılır (sinematik yok; 1. gece rıhtımda, sonraki geceler kontrol odasında). Kayıt yoksa soluk ve tıklanmaz ("No saved night").
  - **SETTINGS**, **QUIT**.
- **Kayıt:** her gece başladığında otomatik (`NLProgress` yuvası: gece numarası + level). Ölünce aynı gece baştan başlar. 7. gece bitince kayıt silinir.
- **Ayarlar (ana menüde ve oyunda):** Master Volume, Ambience Volume (rüzgâr/dalga/yağmur/gök gürültüsü/uzak sesler), Mouse Sensitivity, Invert Mouse Y, Field of View, Brightness (ekran gaması), Graphics Quality (Low–Epic), Window Mode, Resolution, VSync. Değişiklik anında uygulanır, BACK ile kaydedilir (`NLSettings` yuvası + motorun GameUserSettings'i).
- **Oyunda ESC (veya P):** duraklatma menüsü — RESUME, SETTINGS, MAIN MENU, QUIT GAME. Oyun durur. Sigorta kutusu açıkken ESC önce onu kapatır; sinematikte ve ölüyken açılmaz.
  - **Not:** editörde Play (PIE) sırasında ESC oyunu durdurur (editörün kısayolu); orada **P** tuşunu kullan. Standalone/paketlenmiş oyunda ESC çalışır.
- Ayar/kayıt kodu: `UNLGameSubsystem` (oyun ve menü level yolları burada: `GameLevel`, `MenuLevel`). Menüler `NLMenuWidgets` (C++ ile kurulur).
- Test: menü level'inde `-ExecCmds="NLDebugMenuShots"`, oyunda `NLDebugPauseMenu`; devamı denemek için level'i `?NLNoIntro?NLDay=3` ile aç.

## Benzin bidonunu bırakma, taşırken yavaşlama, Boğulmuş'un gece başı beklemesi
- **G:** elindeki benzin bidonunu önüne yere bırakır (eğilip koyar); içindeki benzin korunur, tekrar alınabilir. Kulübedeki bidon yeniden belirmişse yere bırakılan ayrı bir bidon olur. Yeniden belirme her zaman kulübedeki asıl yerinde olur.
- Bidon elindeyken yürüme/koşma hızı `Carry Can Speed Multiplier` (0.82) ile çarpılır (280 -> 230).
- **Boğulmuş:** ilk gece 120 sn (`First Night Appearance Delay`), diğer gecelerin başında 40 sn (`First Appearance Delay`) gelmez.
- Deniz feneri: kapı/pencere parçaları kuleyle birlikte taşınır (önceden taşınınca eski yerlerinde kalıyordu); içerideki fıçı/sandıklar kaldırıldı (`Interior Props`, varsayılan kapalı).
- Tekne: pruva ucu dolu, üstünde pruva güvertesi tahtası (uçtaki boşluklar kapandı).
- Test: `-ExecCmds="NLDebugDropCan"`.

## Fener bekçisi kulübesi, masa/monitör/sandalye, benzin bidonu, ahşap tekne
- **Fenerle birleşik kulübe (Lvl_Ada_Art):** eski gri kontrol binası yerine aynı boyutta kütük kulübe (`NL_ENV_KeeperCabin`, 540 x 420). Kuzeyde giriş kapısı (yanında fener), batı ve güneyde ışıklı pencereler, döşemenin altında eğimli araziyi kapatan temel kütükleri (`Foundation Depth`). Doğu duvarındaki kapı boşluğu deniz fenerine açılır: kule bu duvara yaslandı, kapısı kulübeye döndü, iki döşeme aynı hizada (kulübeden fenerin zemin katına yürünür). Fenerin dışarıya açılan kapısı yok (`Door Panel` kapalı). Masa, monitör, şarj istasyonu ve tavan lambası yerinde.
  - Not: eski kontrol binasının çarpışması yoktu (oyuncu arazide yürüyordu); yeni kulübenin duvarları ve döşemesi çarpışmalı.
- **Güvenlik masası (iki level'de de):** ahşap masa (çekmeceli), eski tüplü (CRT) monitör (ekranı kamera görüntüsü, `Monitor Material Index` = 1), ahşap sandalye (C++ varsayılanı; model yoksa eski küpler).
- **Benzin bidonu:** kırmızı 20 L metal bidon (üçlü sap, ağız, yanlarda kabartma X). Yerdeki `NLFuelCan`, oyuncunun elinde taşıdığı bidon ve Art'ta yakıt alanındaki bidonlar bu model.
- **Açılış teknesi:** kaplama tahtalı ahşap sandal (sivri pruva, V taban, kıç aynalığı), küpeşte, kaburgalar, üç oturak, döşeme tahtaları, dıştan takma motor (dümen kolu), iki kürek, halat, benzin bidonu, pruvada direğe asılı fener. Dümende **Unreal mankeni oturur** (`NLSeatedFigure`: kalça oturakta, ayaklar teknede, sağ el dümen kolunda, sol el dizinde; tekneyle sallanır). Birinci şahsa geçişte gizlenir.
- Modeller `Scripts/nl_env_kit.py` (`NL_KIT_ONLY_NEW=1 NL_KIT=make_boat,make_outboard,make_oar,make_desk,make_crt,make_chair,make_jerrycan`), materyaller `nl_env_materials.py`; level'e `nl_lighthouse_dust.py` (masa/bidon) ve `nl_dress_ada.py` (kulübe, fener, yakıt bidonları) uygular.

## Deniz feneri, fenere yol gösterme, havada toz
- **Fenere nasıl gidileceği (1. gece):**
  1. Tepede dönen fener ışığı ağaçların üstünden her yerden görünüyor; sinematik bitince oyuncu zaten ona doğru bakıyor.
  2. Rıhtımdan çıkışta **"LIGHTHOUSE >"** tabelası, patika boyunca ~11 m arayla **yanan fenerli direkler** var (`NLGuideLanterns`: oyun başında NavMesh'ten rıhtım → güvenlik masası yolunu bulur ve dizer). GameMode > `Guide Lanterns` ile kapatılır.
  3. "DAY 1" yazısı kaybolunca sol üstte soluk bir hedef çıkar: **"Get to the lighthouse."** Masaya yaklaşınca (9 m) ya da oturunca söner, bir daha çıkmaz.
- **Yeni deniz feneri (`NLLighthouse`, Lvl_Ada_Art):** taş kaide, yukarı daralan beyaz/kırmızı bantlı kule, korkuluklu balkon, camlı fener odası, kubbe. Zemin katına açık kapıdan girilir: tavandan sarkan lamba (gölgeli), sandık ve varil, havada toz. Eski blockout kule parçaları ve yaşam alanı kopyadan kaldırıldı; oyuncu başlangıç noktası kontrol odasına alındı.
  - **Işık:** iki huzme 16 sn'de bir döner, 7° aşağı bakar (denize vurur), sisin içinde görünür. **Elektrik kesilince (jeneratör/şalter) fener ve iç lamba söner**, gelince yavaşça yanar: uzaktan da "elektrik gitti" anlaşılır.
  - Ayarlar: `Tower Height`, `Base Radius`, `Rotation Period`, `Beam Pitch`, `Beam Intensity`, `Beam Glow`, `Beam Length`, `Require Power`, `Interior Lamp Intensity`; `Build Tower` kapalıyken sadece dönen ışık olur (Lvl_Ada'da eski kulenin merceğinde böyle).
- **Havada toz (`NLDustVolume`):** kontrol odası, jeneratör binası, (Lvl_Ada'da yaşam alanı), Art'ta ayrıca fener zemin katı, rıhtım kulübesi ve yıkık ev. Zerreler karanlıkta görünmez, fener/lamba vurunca ışıkta süzülür; yavaş salınım materyalde (işlemci yükü yok). Ayarlar: `Extent`, `Density` (m³ başına), `Mote Scale`.
- Üretim: `nl_env_textures.py` → `nl_env_materials.py` (var olan ana materyallere dokunmaz, eksikleri ekler) → `nl_env_kit.py` (`NL_KIT_ONLY_NEW=1`: sadece fener/toz/tabela parçaları) → `nl_lighthouse_dust.py` (Lvl_Ada) → `nl_dress_ada.py` (`NL_ART_RECOPY=1`).
- Test: `-ExecCmds="NLDebugLighthouseTour"` (fener dışı/içi, kontrol odası tozu, rıhtımdan huzme ve yol).

## Gerçek ada: Lvl_Ada_Art (blockout'un dokulu, kütük yapılı kopyası)
- `Lvl_Ada` (blockout) olduğu gibi duruyor; yeni görünüm **`/Game/Levels/Lvl_Ada_Art`** level'inde. Oynanış aktörleri (kapı, sigorta kutusu, jeneratör, kameralar, Runner, Drowned, sinematik) aynı yerde.
- **Rıhtım kulübesi:** referans görseldeki gibi kütük kulübe — köşelerde geçmeli kütükler, kütük alınlık, kiremit sıralı beşik çatı, mahya/aşık kütükleri, soba borusu, ışıklı pencereler (kepenkli), kapının yanında asılı fener (ışık), kapı önünde fıçı ve sandık. Oyun kapısı (NLHideDoor) kapı boşluğuna oturur.
- **İskele:** tahta paneller, suya inen kazıklar, babalar ve halat kangalları, uçta iki fenerli direk (ışıklı).
- **Yıkık ev:** yıkık kütük kulübe (bir duvarı yıkık, çatının yarısı yok, yerde devrik kütükler).
- **Ağaçlar:** orman bloklarının yerine çamlar + patikalara, binalara ve güvenlik kameralarının görüş alanına girmeyecek şekilde ek orman (~130 ağaç, ~%8 kuru çam). **Kıyı:** ~260 kaya.
- **Arazi:** orman zemini; patikalar (eski patika maskesiyle) ıslak çamur; eğimlerde kaya; deniz kıyısında ıslak kum. **Deniz:** koyu, parlak, kayan dalgalar. Deniz feneri sıva/metal, jeneratör ahşap/metal, yakıt tankları paslı metal.
- **Gece:** Lvl_Ada'dan daha karanlık (ay 0.17 lux, pozlama -0.1) ve biraz daha sisli; yere yakın ikinci sis katmanı. Uzak/karanlık yerler fenerle görülür; kulübe ve iskele fenerleri karanlıkta yol gösterir.
- **Performans:** her parça türü tek instanced bileşen (kulübe başına ~10 çizim), ağaçlar/kayalar HISM ve 80–90 m'den sonra çizilmez, kulübe ışıkları gölge atmaz, kayalar/eşyalar gölge atmaz, bulut katmanı kaldırıldı, Lumen yansıması yerine ekran-uzayı yansıma. Ölçüm (aynı makine, başsız): Lvl_Ada ~21 ms/kare, Lvl_Ada_Art ~24 ms/kare.
- **Modüler parçalar (level'e sürükle-bırak):**
  - `NLLogCabin`: Width, Depth, Wall Height, Log Diameter, Roof Pitch, Eave/Gable Overhang, **Openings** (duvar, kayma, en, boy, denizlik; pencere: ışıklı/kepenk; kapı: kanat/fener tarafı), Chimney, Porch Props, lamba ayarları, **Ruined** (duvar yükseklikleri, çatının kalan oranı, eksik kütük olasılığı, devrik kütük sayısı). Ayar değiştirince kendini kurar (Details > NL > **Rebuild Now**).
  - `NLPlankDock`: Length, Width, Post Spacing, Post Depth, Bollard Every, End Lanterns.
  - `NLEnvScatter`: çam (3 çeşit + kuru), kaya (3), fıçı, sandık, kesik kütük — bileşene örnek eklenerek boyanabilir (Foliage değil, HISM).
- **Üretim (sırayla, editör kapalıyken):** `Scripts/nl_env_textures.py` (dokular, ~3 dk) → `Scripts/nl_env_materials.py` → `Scripts/nl_env_kit.py` (meshler) → `Scripts/nl_dress_ada.py` (level; `NL_ART_RECOPY=1` ile Lvl_Ada'dan baştan kopyalar). Kit meshleri GeometryScripting eklentisiyle üretilir (sadece editörde açık).
- Dokular ve meshler kodla üretilmiş geçici çalışmalardır; aynı isimle gerçek asset konursa parçalar onları kullanır.
- Test: `-ExecCmds="NLDebugArtTour"` noktaları gezip ekran görüntüsü alır ve kare süresini loglar.

## Açılış sinematiği: botla rıhtıma yanaşma (ANLIntroDirector)
- Oyun siyahtan açılır, sinema bantları (2.35:1) ve arayüz gizli:
  1. Denizden geniş plan: yağmurda feneri yanan bot adaya doğru gelir, şimşek çakar (~3 sn).
  2. Botun arkasından, dümendeki yolcunun omzunun üstünden: rıhtım ve kulübe yaklaşır (6,5 sn).
  3. Rıhtımın öbür kenarından: bot yavaşlayıp rıhtıma yanaşır, iskeleye tok bir sesle çarpar, motor susar (11,5–16 sn).
  4. Kararma → birinci şahıs: karakter bottan rıhtıma adım atar, adaya döner → **DAY 1** (~23 sn).
- **Boşluk / Enter / Esc** ile geçilir (oyuncu rıhtımda başlar).
- Günün saati ve arızalar (jeneratör yakıtı, şalter, Runner pusuları, Drowned) sinematik bitip DAY 1 başlayınca başlar.
- Ölüp level yeniden açılınca sinematik oynamaz; oyuncu doğrudan rıhtımda başlar, bot iskeleye bağlı durur.
- Level'e `Scripts/nl_setup_intro.py` koyar; rıhtımın (`RIHTIM`) konumunu, yönünü ve ölçülerini kendisi okur. Rıhtım taşınırsa script yeniden çalıştırılmalı.
- Ayarlar (NLIntroDirector): `Arrive Time`, `Shot B Time`, `Shot C Time`, `Step Off Duration`, `Boat Side` (0 = otomatik), `Mooring Along`. Kapatmak için: GameMode > `Play Intro`.
- Bot, motor ve yolcu basit şekillerden yapılmış geçicilerdir. Sesler (`boat_engine_loop`, `boat_engine_stop`, `boat_dock_bump`, `boat_step_off`) kodla üretilmiş geçici seslerdir.
- Otomatik testler (`-ExecCmds`) ve `-NLNoIntro` sinematiği atlar; `-NLIntro` zorla oynatır. `NLDebugIntroShots` sinematik boyunca ekran görüntüsü alır.

## Yağmurlu gece (ANLWeatherDirector)
- 1. gece yağmurla açılır, 2. gece başlarken yağmur 6 sn'de diner. Hangi gecelerin yağmurlu olduğu: `ANLWeatherDirector` > `Rainy Days` (varsayılan sadece `1`). GameMode, level'de yoksa kendisi oluşturur (`bSpawnWeather`).
- Damlalar oyuncunun çevresinde düşer (`Drop Count`, `Area Radius`, `Fall Speed`, `Wind`, `Drop Opacity`, `Drop Glow`). Her damla çatıya/zemine/ağaca değince durur: binaların içine yağmur girmez, kapıdan ve camdan dışarıda yağdığı görünür. Damlalar fener ışığında parlar.
- Materyal: `/Game/FX/NL/M_NLRain` (`Scripts/nl_make_rain.py` ile üretilir).
- Ses: dışarıda `amb_rain_loop`; içeride bu ses boğuklaşır ve çatıya vuran `amb_rain_roof_loop` duyulur.
- Şimşek: 35-80 sn arayla gökyüzü bir an aydınlanır, ardından uzaklığa göre 0,4-3,6 sn sonra gök gürler (`thunder_near`, `thunder_far`, `thunder_far2`). Yağmurda sis 1,6 kat koyulaşır.
- Sesler kodla üretilmiş geçici seslerdir; aynı isimle gerçek kayıt koyarsanız onlar kullanılır.
- Test: konsolda `NLDebugRain 1` / `NLDebugRain 0` yağmuru açar/kapatır, `NLDebugLightning` hemen şimşek çaktırır.

## Teşhis sadece masada (oyuncuyu masaya çeker)
- **Arıza ekranda hiç yazmaz, oyuncu sesten anlar.** Eski "GENERATOR BROKEN / OUT OF FUEL / FUSES SWAPPED" yazıları ve elektrik uyarısı kaldırıldı.
  - **Elektrik kesilince / gelince:** binanın içinden röle "klank" + lambaların uğultusunun pesleşip sönmesi (`power_down`) / yükselip oturması (`power_up`). Her yerden duyulur.
  - **Ne bozuldu:** ses, olduğu yerden ve adanın her yerinden yönüyle gelir (~90 m'ye kadar kısılarak; NLPowerSubsystem > PlayFarEvent):
    - Jeneratör arızası: patlama + motor duruşu (`generator_breakdown` + `generator_stop`), jeneratör binası yönünden.
    - Benzin bitti: sadece motorun öksürüp durması (`generator_stop`).
    - Sabotaj: kıvılcım çıtırtısı + şalter düşmesi (`fuse_trip` + `fuse_switch_off`), sigorta kulübesi yönünden.
  - **Motor uğultusu** artık sadece jeneratörün yakınında duyulur (5 m'den sonra kısılır, 35 m'de biter; NLGenerator > **Hum Hear Distance**). Yanına gidince susmuşsa durmuş demektir.
- **Masadaki monitörde POWER DIAGNOSTICS paneli** (sol orta). Yedek aküyle çalışır, elektrik yokken de (NO SIGNAL ekranında) görünür:
  - `GENERATOR` RUNNING / FAULT - REPAIR ON SITE / STOPPED - NO FUEL
  - `FUEL` yüzde, %20 altında LOW, sıfırda EMPTY - REFUEL
  - `FUSE BOX` OK / WIRING MISMATCH / SWITCH OFF
  - `FUSE WIRING ORDER`: 2×2 renkli kutu (TL/TR/BL/BR = kutuya karşıdan sol üst, sağ üst, sol alt, sağ alt)
- **Sigorta kablolaması her sabotajda değişir** (NLFuseBox > **Randomize Wiring**). Yeni sıra sadece monitörde yazar; oyuncu önce masaya gidip sırayı ezberlemeli, sonra kulübeye koşmalı. Kutudaki ipucu satırı da artık sıranın doğru olup olmadığını söylemez.
- Test: `NLDebugDiag` (dışarıda yazı yok, sesli işaretler logda `NLPower: sound cue`, masada panel, yanlış şalterde geri düşme, monitördeki sırayla düzeltme, jeneratör arızası).

## Fener pili: dışarıda daha hızlı biter
- Oyuncunun üstünde çatı yoksa (açık hava) fener pili **2 kat** hızlı biter: dolu pil dışarıda ~100 sn, içeride ~200 sn ışık verir (NLFlashlightComponent > **Outdoor Drain Multiplier**, **Roof Check Height**).
- Pil sadece kontrol odasındaki **şarj aletinde** ve elektrik varken dolar. Uzun süre dışarıda kalan karanlıkta kalır; Drowned'ı dışarıda el feneriyle kovalamak da pile bağlı.
- Testte ölçülen: dışarıda 10 sn'de %10, içeride 10 sn'de %5.

## Performans notları

Ölçüm: `NLDebugPerf` (Lvl_Ada_Art, 5 tipik nokta; kare/oyun/çizim/GPU süresi ve ProfileGPU dökümü loga yazılır).
Fps sınırı olmadan ölçmek için: `-ExecCmds="t.MaxFPS 0,r.VSync 0,NLDebugPerf"`.

- Güvenlik kamerası (`ANLSecurityCamera::TickFeed`): sadece monitör ekranda görünürken çizer; masada her kare, odadan bakarken ~12 fps, görünmüyorsa hiç. Kamera görüntüsünde Lumen GI/yansıma ve DFAO kapalı (gece görüşünde fark yok).
- Hareketli küçük objeler (yağmur, el feneri, taşınan bidon, fener camı/hüzmeleri, toz, yol fenerleri) uzaklık alanı, Lumen ve ray tracing sahnesine girmez (`bAffectDistanceFieldLighting`, `bAffectDynamicIndirectLighting`, `bVisibleInRayTracing`). Yeni hareketli obje eklerken aynısını yap.
- `DefaultEngine.ini` [SystemSettings]: `r.TranslucencyLightingVolumeDim=48`, `r.Lumen.ScreenProbeGather.TracingOctahedronResolution=6`.
- Lumen yazılım izleme (`r.Lumen.HardwareRayTracing 0`) denendi, bu sahnede daha yavaş; donanımsal kalıyor.

## Sigorta kutusu modeli

Model Blender'da script ile üretilir: `Scripts/blender_fusebox.py` (Steam Blender 5.3).
1. `blender.exe -b --factory-startup --python Scripts/blender_fusebox.py -- <proje>/Saved/NLFuse` (önizleme: `NL_FUSE_PREVIEW=1`, `Saved/NLFuse/preview.png`)
2. Malzemeler: `Scripts/nl_env_materials.py` (MI_NL_FuseCabinet, MI_NL_GaugeFace, MI_NL_Tape, MI_NL_Ink, MI_NL_WarnPlate, MI_NL_Rubber, MI_NL_FuseCeramic, MI_NL_LedGreen)
3. İçe aktarma: `Scripts/nl_import_fusebox.py` -> `/Game/Environment/Kit/SM_NL_FuseCabinet`, `SM_NL_FuseCartridge`, `SM_NL_FuseLever`, `SM_NL_GaugeNeedle`, `SM_NL_FuseLED`

`ANLFuseBox::ApplyModel` modeli ve yerleşimi her açılışta uygular (leveldeki eski küp ayarlarını ezer). Yuvalar: 0-2 üst sıra dikey **CAM 1-3**, 3 yatay **FLASH**; monitördeki kablolama listesi de bu etiketleri kullanır. Ölçüleri değiştirirsen Blender script'indeki yerleşim ile `NLFuseLayout` (NLFuseBox.cpp) aynı kalmalı.
Göstergeler ve yeşil LED'ler sadece genel elektrik durumunu gösterir (hangi sigortanın yanlış olduğunu söylemez).

## Blender modelleri (tekne, jeneratör, bidon, yol feneri)

Ortak yardımcılar `Scripts/blender_nl_common.py`. Hepsi `Saved/NLProps` klasörüne üretilir (önizleme için `NL_PREVIEW=1`):
- `blender_trawler.py`: SM_NL_Trawler (balıkçı teknesi, +X pruva, güverte Z 80) + SM_NL_TrawlerWheel (dümen, X ekseninde döner)
- `blender_old_generator.py`: SM_NL_OldGenerator (açık motorlu eski jeneratör, palet üstünde; kontrol kutusunda durum lambası MI_NL_GenLamp)
- `blender_props.py`: SM_NL_JerryCan (yeşil 20 L), SM_NL_GuidePost (taş kaideli direk), SM_NL_GuideLantern (asılı fener), SM_NL_LanternFlame (mum alevi)
- `blender_lighthouse.py`: SM_NL_LH_Tower (içi boş kule, merkez sütunlu döner merdiven), SM_NL_LH_Gallery (balkon r270 = fener odası zemini, merdiven boşluğu 48..275 derece), SM_NL_LH_LanternRoom (içine girilen fener odası r150, +X kapıdan balkona), SM_NL_LH_House (bitişik sıvalı bekçi evi: önde üçgen alınlıklı giriş, kule tarafında baca, gri taş temel; kontrol masası ve şarj istasyonu içinde), SM_NL_LH_DoorHouse / SM_NL_LH_DoorTower (açılır kapı kanatları: ANLSwingDoor, E ile; oyuncu olmayan karakter yaklaşınca kendiliğinden açılır). Kule dışı MI_NL_LighthousePaint (kırmızı-beyaz bant), iç ve süsler MI_NL_World_Plaster. Araziye oturtma: `nl_lighthouse_ground.py` (zemini arazinin en yüksek noktasının üstüne alır, içerideki aktörleri birlikte kaydırır). Ev ve kule girişleri ayrı: kulenin kendi dış kapısı 300 derecede (DOOR_A), ev ile kule arasındaki duvar kapalı. İçe aktarma: `NL_MODELS=SM_NL_LH_Tower,SM_NL_LH_Gallery,SM_NL_LH_LanternRoom,SM_NL_LH_House`, ardından `nl_lighthouse_house.py` (eski kütük kulübeleri ve eski modelleri siler). Ölçüler oyuncu kapsülüne göre (r40): fener odasında lens ile cam arası ve balkon yürünebilir. Test: `NLDebugClimb` (merdivenden tepeye çıkar, fener odası ve balkon kareleri).

Sonra `nl_env_materials.py` ve `nl_import_models.py` (içe aktarır, adadaki jeneratörü SM_NL_OldGenerator'a geçirir; `NL_MODELS=SM_NL_Trawler,...` ile sadece seçilenler).

Açılış (NLIntroDirector): kaptan (sarı yağmurluk) köşkte iki eli dümende, dümen dönüşlere göre döner; yolcu (koyu mont) arka güvertede oyuncunun yanında, bir eli küpeştede. Oyuncu rıhtıma inince dönüp tekneye bakar, yolcu el sallar; sonra adaya döner (DAY 1) ve tekne motoru açıp denize çıkarak kaybolur (DepartDuration). Ölüm/devam ile başlarken tekne yoktur. Figürler: ANLSeatedFigure (bSeated=false ayakta, SetHandTargetsWorld, SetWave).
Yol fenerleri (NLGuideLanterns): ışık mum gibi yumuşak dalgalanır, hiç sönmez (FlickerStrength); alev boy atar, fener rüzgârda hafifçe sallanır. Test: `NLDebugIntroShots` (`-NLIntro` ile), `NLDebugGuideShots`, `NLDebugGenShots`.
