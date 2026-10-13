# Таблица соответствий / Compatibility matrix

**«С чем, кого, почему»** — главный документ проекта.
The **what / which / why** matrix — the central document of this project.

> **Как читать:** слева направо — от того, что **реально стоит в телефоне**,
> к тому, что **написано в DT**, и к тому, **какой драйвер** на это подхватится.
> Между ними несовпадение — и именно в этом комментарии объясняется,
> почему оно осталось таким.
>
> **How to read:** left to right, from what the phone **actually reports**, to what is
> **written in the DT**, to which **driver** binds to it. The mismatch in between is
> intentional — and that is exactly what the notes explain.

---

## 1. Сводная таблица / Summary table

Источник реальных строк — DT, который телефон отдаёт сам:
`/sys/firmware/fdt` на живом Android.
Real strings were taken from the live device: `/sys/firmware/fdt` on running Android.

| Подсистема / Subsystem | В телефоне / On device | У нас в DTS / In our DTS | Драйвер / Driver | ПОЧЕМУ / Why |
|---|---|---|---|---|
| **SD-карта** / SD | `mediatek,mt6789-mmc` | `mediatek,mt8183-mmc` | `drivers/mmc/mtk-sd.c`<br>→ `mt8183_compat` | ⚠️ В драйвере **нет** строки `mt6789-mmc` вообще. Список: mt7620/21/22/23, mt7981/86/87, mt8512/16, **mt8183**. С реальной строкой контроллер не поднимется вообще. `mt8183_compat` — единственный вариант, содержащий нужные для MT6789 патчи |
| **UFS** | `mediatek,mt8183-ufshci` | `mediatek,mt8183-ufshci` | `drivers/ufs/ufs-mediatek.c` | ✅ Совпадает с реальным DT. Драйвер принимает и `mt6789-ufshci`, и `mt8183-ufshci` — код один и тот же (`ufs_mtk_probe`), выбран вариант реального устройства |
| **USB 2.0** / USB2 | `mediatek,mt6789-usb20` | `mediatek,mt8518-musb`<br>+ `mediatek,mtk-musb` | `drivers/usb/musb-new/mt85xx.c` | ⚠️ `musb-new` биндится **только** на `mt8518-musb` (`mt85xx.c:432`). Драйвера под `mt6789-usb20` в форке нет — это другое семейство. Ядро MUSB совпадает, отличается обвязка PHY |
| **USB PHY** / T-PHY | `mt6789-tphy` | `mediatek,mt6789-tphy`<br>+ `generic-tphy-v2` | `drivers/phy/mediatek/` | ✅ В формковом `mt6789.dtsi` был `generic-tphy-v2` с `reg = <0 0x11f40000>` и `#address-cells = <2>`. Исправлено: `ranges = <0 0 0x11f40000 0x1000>`, `#address-cells = <1>` — иначе PHY не зондируется |
| **Дисплей** / Display | — | `mediatek,mt6789-disp` | `drivers/video/mediatek/mtk_disp_g99.c`<br>**собственный** | ✅ В форке нет DRM-драйвера для MT6789.LK/BL2_EXT уже поднимает DSI и панель, наш драйвер перенастраивает конвейер OVL0→RDMA0→DITHER0→DSI0 под framebuffer U-Boot. Подробно: [`hardware/display.md`](hardware/display.md) |
| **Кнопки** / Buttons | `mediatek,mt6779-keypad` | `mediatek,mt6779-keypad` | `drivers/button/button-mtk-kp.c`<br>**собственный** | ✅ Драйвера KP в форке не было — только `button-adc`, `button-gpio`, `button-qcom-pmic`, ни один не подходит к матричной клавиатуре MTK. Написан на основе Linux `mt6779-keypad.c`. Подробно: [`hardware/buttons.md`](hardware/buttons.md) |
| **Сторожевой таймер** / WDT | `mediatek,mt6789-wdt` | не описан в DTS | — | ⏸ Узел `wdt@` отсутствует и в формковом `mt6789.dtsi`. В U-Boot WDT не используется, поэтому не трогали |

---

## 2. Почему так много расхождений / Why so many mismatches

Тут действуют три независимых источника правды, и они не совпадают:

```text
   реальный DT телефона              формковый mt6789.dtsi            драйверы U-Boot
   (то, что делает Android)          (то, что прислал ellyq)          (то, на что биндится)
            │                                 │                                │
            │   mt6789-mmc                     │   mt6789-mmc                   │  mt8183-mmc   ← ← ←
            │   mt6789-usb20                   │   mt8518-musb                  │  mt8518-musb
            │   mt6789-wdt                     │   mt8781-wdt                   │  (нет)
            │   mt8183-ufshci                  │   mt6789-ufshci                │  оба
            │   mt6789-tphy                    │   generic-tphy-v2              │  mt6789-tphy
            └────────────┬────────────────────┴───────────────┬────────────────┘
                          │                                    │
                   ЭТА ЖЕЛЕЗКА                        ЭТО БЫЛО БЫ, ЕСЛИ БЫ
                   ↓                                    ↓
                   ↓         ┌───────────────────────┐      ↓
                   └────────►│  наш DTS: компромисс  │◄─────┘
                             │  максимально близко   │
                             │  к железу И к драйверам│
                             └───────────────────────┘
```

Главное правило проекта:

> **Приоритет — чтобы заработало.**
> 1. Железо и реальное поведение (живой Android) — высший приоритет.
> 2. Возможность драйвера U-Boot биндиться — второе.
> 3. Формальная «правильность» строки в dtsi — последнее.

> **Priority: make it work.**
> 1. Hardware and observed behaviour (live Android) — highest.
> 2. Whether the U-Boot driver can bind — second.
> 3. Formal correctness of the string in the dtsi — last.

---

## 3. Отличия от формкового `mt6789.dtsi` / Changes vs upstream dtsi

Файл: `arch/arm/dts/mt6789.dtsi` — единственный **общий** файл форка, который правится
под конкретное устройство (остальное — в `mt6789-infinix-x676b.dts`).

| Что / What | Форк / Upstream | У нас / Ours | Причина / Reason |
|---|---|---|---|
| USB `reg` | `<0 0x11200000 0 0x10000>` + `<0 0x11200000 0 0x10000>` (`mac`, `ippc`) | `<0x11200000 0x10000>` | Драйвер `mt85xx.c` не использует `reg-names`; второй регион не нужен |
| USB `clock-names` | `main`, `mcu`, `univpll` | `usbpll`, `usbmcu`, `usb` | Порядок/имена проверяются драйвером по позиции; старые имена не биндятся к нужным гейтам MT6789 |
| T-PHY `compatible` | `generic-tphy-v2` | `mt6789-tphy`, `generic-tphy-v2` | Нужен MTK-специфичный путь (`t-phy` регистры отличаются) |
| T-PPHY `reg` | `<0 0x11f40000 0 0x1000>`, cells = 2 | `ranges = <0 0 0x11f40000 0x1000>`, cells = 1 | Схема адресации через `ranges`, иначе PHY не находится |
| T-PHY `reg` (порты) | `<0 0x700>` | `<0x0 0x700>` | Требование драйвера к размеру ячейки |
| SD `compatible` | `mt8183-mmc` | без изменений | Совпадает с нашим выбором |
| SD `clocks` | закомментировано | `CLK_TOP_MSDC30_1_SEL`, `CLK_IFRAO_MSDC1`, `CLK_IFRAO_MSDC1_SRC` | Без них контроллер не тактируется — v189 это и чинил |
| SD `bus-width` | нет | `<4>` | 4-битный SD |
| SD `vmmc`/`vqmmc` | нет | два `regulator-fixed` (3.3 V / 1.8–3.3 V) | Модель питания; реальное напряжение — из дампа Android |
| SD pinctrl | нет | `mmc1_pins_default`: CMD/DAT/CLK | Режим, IES, SMT, PU/PD, drive-strength — v179 |
| SD `no-mmc`, `no-sdio` | нет | заданы | Карта именно **SD**, не eMMC и не SDIO |
| SD `builtin-cd` | нет | `<0>` | Активный уровень CD — v189 |

---

## 4. Совместимость по подсистемам / Subsystem compatibility

| Подсистема | Статус | Где смотреть / See |
|---|---|---|
| USB fastboot | ✅ работает | [`hardware/usb.md`](hardware/usb.md) |
| USB ACM (serial по USB) | ✅ работает | [`hardware/usb.md`](hardware/usb.md) |
| UMS (mass storage) | ✅ работает через `scsi` | [`hardware/usb.md`](hardware/usb.md) |
| Дисплей (текст) | ✅ работает | [`hardware/display.md`](hardware/display.md) |
| Кнопки (Volume Up/Down) | ✅ работают, самописный драйвер | [`hardware/buttons.md`](hardware/buttons.md) |
| UFS (внутренняя память) | ✅ работает | этот документ, §1 |
| SD-карта | ❌ не работает — **блокер** | [`hardware/sd-card-msdc1.md`](hardware/sd-card-msdc1.md) |
| Загрузка Android | ❌ паника `Attempted to kill init` | не тот DTB, работы не начаты |

---

## 5. Полезные команды / Handy commands

```bash
# Реальные строки DT с живого телефона (нужен root)
adb shell su -c 'strings /sys/firmware/fdt | grep -i mediatek'

# Что U-Boot реально подхватил
#   в консоли U-Boot:
setenv fdt_addr_r 0x43fff000   # адрес FDT, если нужен свой
printenv

# Проверить, какой драйвер поднялся
#   в консоли U-Boot:
dm tree        # дерево драйверов
dm dev         # список устройств
```

---

## 6. Связанные документы / Related

- Решения с обоснованием: [`decisions/`](decisions/)
- Дисплей: [`hardware/display.md`](hardware/display.md)
- USB: [`hardware/usb.md`](hardware/usb.md)
- SD-карта: [`hardware/sd-card-msdc1.md`](hardware/sd-card-msdc1.md)
- Откуда взяты данные: [`reverse-engineering/`](reverse-engineering/)