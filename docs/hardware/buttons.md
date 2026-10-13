# Кнопки / Buttons

**Статус: ✅ работает.** Volume Up и Volume Down нажимаются, ими управляется
bootmenu — отдельный UART-кабель не нужен.
**Status: ✅ working.** Volume Up and Volume Down drive the bootmenu — no UART
cable required.

---

## ЧТО / What

| | |
|---|---|
| Контроллер | MediaTek KP (keypad), база **`0x10010000`** |
| Драйвер | **`drivers/button/button-mtk-kp.c` — самописный, 204 строки** |
| Основан на | `drivers/input/keyboard/mt6779-keypad.c` из ядра Linux |
| Подсистема U-Boot | `button-uclass` (`CONFIG_BUTTON=y`) |
| Совместимость в DTS | `mediatek,mt6779-keypad` |
| Конфиг | `CONFIG_BUTTON=y`, `CONFIG_BUTTON_MTK_KP=y`,<br>`CONFIG_BUTTON_REMAP_PHONE_KEYS=y` |

> ⚠️ **В форке `ellyq/u-boot-mtk-g99` этого драйвера не было.**
> В U-Boot были только `button-adc`, `button-gpio` и `button-qcom-pmic` —
> ни один не подходит к матричной клавиатуре MTK.
>
> **There was no such driver in the `ellyq` fork.** U-Boot only had `button-adc`,
> `button-gpio` and `button-qcom-pmic` — none of which fit the MTK matrix keypad.

---

## Кто чем занимается / Who does what

| Файл | Задача |
|---|---|
| `drivers/button/button-mtk-kp.c` | **написан с нуля**: чтение матрицы, распознавание нажатий, включение гейтов |
| `arch/arm/dts/mt6789-infinix-x676b.dts` | узлы `volume-up` и `volume-down` |
| `board/mediatek/mtk-phone.env` | подключение `button-kbd` к `stdin` в `preboot` |
| `drivers/button/Kconfig`, `Makefile` | регистрация нового драйвера |

---

## Как это работает / How it works

### Регистры контроллера

| Регистр | Смещение | Что |
|---|---|---|
| `KP_MEM1` | `0x0004` | матрица, биты [15:0] |
| `KP_MEM2` | `0x0008` | |
| `KP_MEM3` | `0x000c` | |
| `KP_MEM4` | `0x0010` | |
| `KP_MEM5` | `0x0014` | |

**Активный уровень — низкий:** `0` = нажато, `1` = отпущено.
Всего 136 позиций (4 × 32 + 8).

### Узлы в DTS

Обе кнопки читаются из одного регистра — различаются значением матрицы:

```dts
volume-down {
        compatible = "mediatek,mt6779-keypad";
        reg = <0x10010000 0x1000>;
        label = "vdown";
        linux,code = <KEY_VOLUMEDOWN>;    /* 114 */
        mediatek,matrix-val = <0xbfdf>;
        mediatek,matrix-reg = <0x0c>;     /* KP_MEM3 */
        status = "okay";
};

volume-up {
        compatible = "mediatek,mt6779-keypad";
        reg = <0x10010000 0x1000>;
        label = "vup";
        linux,code = <KEY_VOLUMEUP>;      /* 115 */
        mediatek,matrix-val = <0xdfef>;
        mediatek,matrix-reg = <0x0c>;     /* KP_MEM3 */
        status = "okay";
};
```

Коды клавиш: Volume Down = `114`, Volume Up = `115`.

### Побочный эффект: гейты часов

Драйвер заодно **включает гейты INFRACFG_AO** для периферии загрузки —
иначе контроллер клавиатуры не читается:

| Группа | Базовый адрес | SET / CLR / STA |
|---|---|---|
| IFRAO0 | `0x10001000` | `+0x80` / `+0x84` / `+0x90` |
| IFRAO1 | `0x10001000` | `+0x88` / `+0x8c` / `+0x94` |
| IFRAO2 | `0x10001000` | `+0xa4` / `+0xa8` / `+0xac` |
| IFRAO4 | `0x10001000` | `+0xe0` / `+0xe4` / `+0xe8` |
| IFRAO5 | `0x10001000` | `+0xd0` / `+0xd4` / `+0xd8` |

> 💡 Гейты включаются через **SET**, а не CLR. В SD-драйвере наоборот —
> обход зависания шины `infra_ao` пришлось делать через CLR. Разные регионы
> ведут себя по-разному, см. [`sd-card-msdc1.md`](sd-card-msdc1.md).

---

## Как это используется / Usage

### Подключение к консоли

```kconfig
preboot=setenv stdin serial,button-kbd; setenv stdout serial,vidconsole; setenv stderr serial,vidconsole; scsi reset; scsi scan
```

`button-kbd` добавляется в `stdin` — кнопки работают вместе с serial
и выводом на экран.

### Управление bootmenu

| Кнопка | Действие |
|---|---|
| **Volume Up** | Enter — выбрать пункт |
| **Volume Down** | скролл вниз |

Навигация зацикливается: с последнего пункта — на первый, с первого — на последний.

### Проверка

```bash
# в консоли U-Boot
button list
button vup
button vdown
```

---

## Контекст / How it came about

| Версия | Что сделано |
|---|---|
| v88 | драйвер заработал — `button vup` / `button vdown` корректно определяют нажатие |
| v89 | кнопки подключены к `bootmenu` |

Коммиты: `9501b443546` (регистры из Linux-драйвера), `167d98d575c` (интеграция в bootmenu).

---

## Связанное / Related

- Регистры KP: `drivers/button/button-mtk-kp.c`
- Узлы: `arch/arm/dts/mt6789-infinix-x676b.dts`, строки 210–230
- Окружение: `board/mediatek/mtk-phone.env`
- Совместимость строк: [`../compatibility.md`](../compatibility.md)