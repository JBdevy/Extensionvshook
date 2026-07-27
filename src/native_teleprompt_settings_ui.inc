// Interface nativa das configuracoes do Teleprompt.
// Este arquivo e incluido depois das estruturas e funcoes de persistencia do
// Teleprompt para compartilhar o mesmo estado, sem bridge HTTP ou Electron.

struct NativeTelepromptSettingsUiHit {
  RECT rect{0, 0, 0, 0};
  std::string action;
};

struct NativeTelepromptSettingsUiState {
  HWND hwnd = nullptr;
  int page = 0; // 0=TP1, 1=TP2, 2=Recados
  int scroll = 0;
  int maxScroll = 0;
  RECT viewport{0, 0, 0, 0};
  RECT scrollTrack{0, 0, 0, 0};
  RECT scrollThumb{0, 0, 0, 0};
  bool scrollDragging = false;
  int scrollDragStartY = 0;
  int scrollDragStartValue = 0;

  NativeTelepromptSettings teleprompt[2];
  NativeTechnicalNoticeSettings notice;
  std::vector<NativeTelepromptSettingsUiHit> hits;

  bool scaleDragging = false;
  bool scaleDirty = false;
  std::string scaleKey;
  RECT scaleRect{0, 0, 0, 0};

  std::string dropdownKey;
  RECT dropdownAnchor{0, 0, 0, 0};

  bool colorOpen = false;
  bool colorNotice = false;
  std::string colorKey;
  std::string colorOriginal;
  std::string colorHexInput;
  bool colorHexFocused = false;
  bool colorHexReplaceOnType = false;
  int colorR = 255;
  int colorG = 234;
  int colorB = 0;
  int colorChannelDragging = -1;
  RECT colorChannelRect{0, 0, 0, 0};

  bool noticeEmojiFocused = false;
  std::string status = "Configuracoes carregadas.";
};

static NativeTelepromptSettingsUiState
  g_nativeTelepromptSettingsUi;
static std::unique_ptr<WDL_WinMemBitmap>
  g_nativeTelepromptSettingsUiBackBuffer;
static int g_nativeTelepromptSettingsUiBackBufferWidth = 0;
static int g_nativeTelepromptSettingsUiBackBufferHeight = 0;

struct NativeTelepromptSettingsUiField {
  const char* key;
  const char* label;
};

struct NativeTelepromptSettingsUiScaleField {
  const char* key;
  const char* label;
  double minimum;
  double maximum;
  double step;
};

static const NativeTelepromptSettingsUiField
  kNativeTelepromptSettingsUiColors[] = {
    {"textColor", "Cor da letra"},
    {"textBoxColor", "Cor do contorno da letra"},
    {"clockColor", "Cor do cronometro"},
    {"clockExpiredColor", "Cor do fim regressivo"},
    {"clockBorderColor", "Cor da borda do cronometro"},
    {"localClockColor", "Cor do relogio local"},
    {"borderColor", "Cor da borda da janela"},
    {"songNameColor", "Cor do nome da musica"},
    {"queueNameColor", "Cor do nome da fila"},
    {"progressColor", "Cor do progresso"}
  };

static const NativeTelepromptSettingsUiScaleField
  kNativeTelepromptSettingsUiScales[] = {
    {"textScale", "Escala da letra", 0.50, 1.00, 0.05},
    {"clockScale", "Escala do cronometro", 0.50, 1.00, 0.05},
    {"songNameScale", "Escala do nome da musica", 0.50, 2.00, 0.05},
    {"queueNameScale", "Escala do nome da fila", 0.50, 2.00, 0.05},
    {"mediaScale", "Escala da midia", 0.50, 1.50, 0.05},
    {"previewScale", "Profundidade Preview", 0.50, 1.00, 0.05},
    {"localClockDepth", "Escala do relogio local", 0.50, 2.00, 0.05}
  };

static const NativeTelepromptSettingsUiField
  kNativeTelepromptSettingsUiChoices[] = {
    {"textCase", "Caixa da letra"},
    {"fontFamily", "Fonte da letra"},
    {"songNameFontFamily", "Fonte do nome da musica"},
    {"queueNameFontFamily", "Fonte do nome da fila"},
    {"clockPosition", "Posicao do cronometro"},
    {"localClockPosition", "Posicao do relogio local"},
    {"songNamePosition", "Posicao do nome da musica"},
    {"queueNamePosition", "Posicao do nome da fila"},
    {"progressPosition", "Posicao do progresso"}
  };

static const NativeTelepromptSettingsUiField
  kNativeTelepromptSettingsUiToggles[] = {
    {"windowBorderEnabled", "Mostrar borda da janela"},
    {"rgbWindowBorderEnabled", "RGB borda da janela"},
    {"rgbClockBorderEnabled", "RGB borda do cronometro"},
    {"rgbTextBoxBorderEnabled", "RGB borda da letra"},
    {"clockBorderEnabled", "Mostrar borda do cronometro"},
    {"textBoxEnabled", "Mostrar borda da letra"},
    {"clockEnabled", "Mostrar cronometro"},
    {"localClockEnabled", "Mostrar relogio local"},
    {"songNameEnabled", "Mostrar nome da musica"},
    {"queueNameEnabled", "Mostrar nome da fila"},
    {"previewEnabled", "Mostrar Preview nesta janela"},
    {"progressEnabled", "Mostrar progresso da letra"}
  };

static const NativeTelepromptSettingsUiField
  kNativeTelepromptSettingsUiNoticeColors[] = {
    {"textColor", "Cor da letra"},
    {"backgroundColor", "Cor de fundo"},
    {"flashColor", "Cor do pisca"}
  };

static const NativeTelepromptSettingsUiField
  kNativeTelepromptSettingsUiNoticeToggles[] = {
    {"window1Enabled", "Mostrar no Teleprompt 1"},
    {"window2Enabled", "Mostrar no Teleprompt 2"},
    {"emojiEnabled", "Mostrar simbolo no recado"},
    {"cleanDisplay", "Mostrar recados limpo"}
  };

static bool nativeTelepromptSettingsUiIsOpen()
{
  return g_nativeTelepromptSettingsUi.hwnd &&
    IsWindow(g_nativeTelepromptSettingsUi.hwnd);
}

static void nativeTelepromptSettingsUiReleaseBackBuffer()
{
  g_nativeTelepromptSettingsUiBackBuffer.reset();
  g_nativeTelepromptSettingsUiBackBufferWidth = 0;
  g_nativeTelepromptSettingsUiBackBufferHeight = 0;
}

static HDC nativeTelepromptSettingsUiBuffer(
  HDC paintDc,
  int width,
  int height)
{
  if (!paintDc || width <= 0 || height <= 0) return nullptr;
  if (g_nativeTelepromptSettingsUiBackBuffer &&
      g_nativeTelepromptSettingsUiBackBufferWidth == width &&
      g_nativeTelepromptSettingsUiBackBufferHeight == height) {
    return g_nativeTelepromptSettingsUiBackBuffer->GetDC();
  }
  nativeTelepromptSettingsUiReleaseBackBuffer();
  g_nativeTelepromptSettingsUiBackBuffer =
    std::make_unique<WDL_WinMemBitmap>();
  g_nativeTelepromptSettingsUiBackBuffer->DoSize(
    paintDc, width, height);
  HDC dc = g_nativeTelepromptSettingsUiBackBuffer->GetDC();
  if (!dc) {
    nativeTelepromptSettingsUiReleaseBackBuffer();
    return nullptr;
  }
  g_nativeTelepromptSettingsUiBackBufferWidth = width;
  g_nativeTelepromptSettingsUiBackBufferHeight = height;
  return dc;
}

static POINT nativeTelepromptSettingsUiPoint(LPARAM lParam)
{
  POINT point{};
  point.x = static_cast<short>(LOWORD(lParam));
  point.y = static_cast<short>(HIWORD(lParam));
  return point;
}

static bool nativeTelepromptSettingsUiRectVisible(
  const RECT& rect)
{
  const RECT& view = g_nativeTelepromptSettingsUi.viewport;
  return rect.right > view.left && rect.left < view.right &&
    rect.bottom > view.top && rect.top < view.bottom;
}

static void nativeTelepromptSettingsUiAddHit(
  const RECT& rect,
  const std::string& action,
  bool clipToViewport = false)
{
  RECT hit = rect;
  if (clipToViewport) {
    const RECT& view = g_nativeTelepromptSettingsUi.viewport;
    hit.left = std::max(hit.left, view.left);
    hit.top = std::max(hit.top, view.top);
    hit.right = std::min(hit.right, view.right);
    hit.bottom = std::min(hit.bottom, view.bottom);
  }
  if (hit.right <= hit.left || hit.bottom <= hit.top) return;
  g_nativeTelepromptSettingsUi.hits.push_back({hit, action});
}

static bool nativeTelepromptSettingsUiHover(
  const RECT& rect)
{
  if (!nativeTelepromptSettingsUiIsOpen()) return false;
  POINT cursor{};
  if (!nativeUiGetCursorPosition(&cursor)) return false;
  ScreenToClient(g_nativeTelepromptSettingsUi.hwnd, &cursor);
  return PtInRect(&rect, cursor) != 0;
}

static void nativeTelepromptSettingsUiDrawButton(
  HDC dc,
  const RECT& rect,
  const std::string& label,
  bool active,
  bool enabled = true,
  COLORREF activeColor = RGB(28, 168, 92))
{
  COLORREF fill = enabled
    ? (active ? activeColor : RGB(54, 61, 72))
    : RGB(35, 40, 48);
  COLORREF border = enabled
    ? (active ? RGB(235, 235, 235) : RGB(92, 101, 115))
    : RGB(58, 64, 74);
  COLORREF text = enabled
    ? (active ? RGB(12, 18, 24) : RGB(239, 243, 247))
    : RGB(111, 120, 132);
  if (enabled && nativeTelepromptSettingsUiHover(rect)) {
    fill = active ? RGB(48, 190, 110) : RGB(70, 79, 92);
  }
  nativeAppActiveFillRoundRect(dc, rect, fill, border, 4);
  nativeAppActiveDrawText(dc, label, rect,
    DT_CENTER | DT_VCENTER | DT_SINGLELINE |
      DT_END_ELLIPSIS | DT_NOPREFIX,
    text, nativeUiSharedFont());
}

static std::string* nativeTelepromptSettingsUiTpColor(
  NativeTelepromptSettings& settings,
  const std::string& key)
{
  if (key == "textColor") return &settings.textColor;
  if (key == "textBoxColor") return &settings.textBoxColor;
  if (key == "clockColor") return &settings.clockColor;
  if (key == "clockExpiredColor") return &settings.clockExpiredColor;
  if (key == "clockBorderColor") return &settings.clockBorderColor;
  if (key == "localClockColor") return &settings.localClockColor;
  if (key == "borderColor") return &settings.borderColor;
  if (key == "songNameColor") return &settings.songNameColor;
  if (key == "queueNameColor") return &settings.queueNameColor;
  if (key == "progressColor") return &settings.progressColor;
  return nullptr;
}

static std::string* nativeTelepromptSettingsUiNoticeColor(
  NativeTechnicalNoticeSettings& settings,
  const std::string& key)
{
  if (key == "textColor") return &settings.textColor;
  if (key == "backgroundColor") return &settings.backgroundColor;
  if (key == "flashColor") return &settings.flashColor;
  return nullptr;
}

static double* nativeTelepromptSettingsUiScale(
  NativeTelepromptSettings& settings,
  const std::string& key)
{
  if (key == "textScale") return &settings.textScale;
  if (key == "clockScale") return &settings.clockScale;
  if (key == "songNameScale") return &settings.songNameScale;
  if (key == "queueNameScale") return &settings.queueNameScale;
  if (key == "mediaScale") return &settings.mediaScale;
  if (key == "previewScale") return &settings.previewScale;
  if (key == "localClockDepth") return &settings.localClockDepth;
  return nullptr;
}

static bool* nativeTelepromptSettingsUiTpToggle(
  NativeTelepromptSettings& settings,
  const std::string& key)
{
  if (key == "windowBorderEnabled") {
    return &settings.windowBorderEnabled;
  }
  if (key == "rgbWindowBorderEnabled") {
    return &settings.rgbWindowBorderEnabled;
  }
  if (key == "rgbClockBorderEnabled") {
    return &settings.rgbClockBorderEnabled;
  }
  if (key == "rgbTextBoxBorderEnabled") {
    return &settings.rgbTextBoxBorderEnabled;
  }
  if (key == "clockBorderEnabled") return &settings.clockBorderEnabled;
  if (key == "textBoxEnabled") return &settings.textBoxEnabled;
  if (key == "clockEnabled") return &settings.clockEnabled;
  if (key == "localClockEnabled") return &settings.localClockEnabled;
  if (key == "songNameEnabled") return &settings.songNameEnabled;
  if (key == "queueNameEnabled") return &settings.queueNameEnabled;
  if (key == "previewEnabled") return &settings.previewEnabled;
  if (key == "progressEnabled") return &settings.progressEnabled;
  return nullptr;
}

static bool* nativeTelepromptSettingsUiNoticeToggle(
  NativeTechnicalNoticeSettings& settings,
  const std::string& key)
{
  if (key == "window1Enabled") return &settings.window1Enabled;
  if (key == "window2Enabled") return &settings.window2Enabled;
  if (key == "emojiEnabled") return &settings.emojiEnabled;
  if (key == "cleanDisplay") return &settings.cleanDisplay;
  return nullptr;
}

static std::string* nativeTelepromptSettingsUiTpChoice(
  NativeTelepromptSettings& settings,
  const std::string& key)
{
  if (key == "textCase") return &settings.textCase;
  if (key == "fontFamily") return &settings.fontFamily;
  if (key == "songNameFontFamily") {
    return &settings.songNameFontFamily;
  }
  if (key == "queueNameFontFamily") {
    return &settings.queueNameFontFamily;
  }
  if (key == "clockPosition") return &settings.clockPosition;
  if (key == "localClockPosition") {
    return &settings.localClockPosition;
  }
  if (key == "songNamePosition") return &settings.songNamePosition;
  if (key == "queueNamePosition") {
    return &settings.queueNamePosition;
  }
  if (key == "progressPosition") return &settings.progressPosition;
  return nullptr;
}

static std::vector<std::pair<std::string, std::string>>
nativeTelepromptSettingsUiChoiceOptions(const std::string& key)
{
  if (key == "textCase") {
    return {
      {"original", "Padrao (como foi escrito)"},
      {"uppercase", "CAIXA ALTA"},
      {"lowercase", "caixa baixa"}
    };
  }
  if (key == "clockPosition" ||
      key == "songNamePosition" ||
      key == "queueNamePosition" ||
      key == "progressPosition") {
    return {{"top", "Em cima"}, {"bottom", "Em baixo"}};
  }
  if (key == "localClockPosition") {
    return {{"left", "Esquerda"}, {"right", "Direita"}};
  }
  return {
    {"Arial", "Arial"},
    {"Segoe UI", "Segoe UI"},
    {"Verdana", "Verdana"},
    {"Tahoma", "Tahoma"},
    {"Georgia", "Georgia"},
    {"Trebuchet MS", "Trebuchet MS"},
    {"Impact", "Impact"}
  };
}

static std::string nativeTelepromptSettingsUiChoiceLabel(
  const std::string& key,
  const std::string& value)
{
  for (const auto& option :
       nativeTelepromptSettingsUiChoiceOptions(key)) {
    if (option.first == value) return option.second;
  }
  return value;
}

static bool nativeTelepromptSettingsUiParseHex(
  std::string value,
  int& red,
  int& green,
  int& blue)
{
  value = nativeTrim(value);
  if (!value.empty() && value.front() == '#') {
    value.erase(value.begin());
  }
  if (value.size() != 6) return false;
  for (char character : value) {
    if (!std::isxdigit(
          static_cast<unsigned char>(character))) {
      return false;
    }
  }
  char* end = nullptr;
  const unsigned long packed =
    std::strtoul(value.c_str(), &end, 16);
  if (!end || *end != '\0') return false;
  red = static_cast<int>((packed >> 16) & 0xff);
  green = static_cast<int>((packed >> 8) & 0xff);
  blue = static_cast<int>(packed & 0xff);
  return true;
}

static std::string nativeTelepromptSettingsUiHex(
  int red,
  int green,
  int blue)
{
  char text[16] = "";
  std::snprintf(text, sizeof(text), "#%02X%02X%02X",
    std::max(0, std::min(255, red)),
    std::max(0, std::min(255, green)),
    std::max(0, std::min(255, blue)));
  return text;
}

static void nativeTelepromptSettingsUiInvalidateTeleprompts()
{
  for (int index = 0; index < 2; ++index) {
    HWND hwnd = g_nativeTelepromptWindows[index].hwnd;
    if (hwnd && IsWindow(hwnd)) {
      InvalidateRect(hwnd, nullptr, FALSE);
    }
  }
}

static void nativeTelepromptSettingsUiCommitTp(int index)
{
  index = index == 1 ? 1 : 0;
  const int slot = index + 1;
  nativeTelepromptSaveSettings(
    slot,
    nativeTelepromptSettingsToJson(
      slot, g_nativeTelepromptSettingsUi.teleprompt[index]));
  g_nativeTelepromptSettingsUi.status =
    "Teleprompt " + std::to_string(slot) +
    " salvo automaticamente.";
}

static void nativeTelepromptSettingsUiCommitNotice()
{
  nativeTechnicalNoticeLoadState();
  {
    std::lock_guard<std::mutex> lock(g_nativeMutex);
    g_nativeTechnicalNoticeSettings =
      g_nativeTelepromptSettingsUi.notice;
  }
  g_nativeTechnicalNoticeSettingsDirty.store(true);
  nativeTechnicalNoticeFlushPersistenceOnMainThread();
  nativeTelepromptSettingsUiInvalidateTeleprompts();
  g_nativeTelepromptSettingsUi.status =
    "Configuracoes dos Recados salvas automaticamente.";
}

static void nativeTelepromptSettingsUiLoadDrafts()
{
  nativeTelepromptLoadSettings();
  nativeTechnicalNoticeLoadState();
  std::lock_guard<std::mutex> lock(g_nativeMutex);
  g_nativeTelepromptSettingsUi.teleprompt[0] =
    g_nativeTelepromptSettings[0];
  g_nativeTelepromptSettingsUi.teleprompt[1] =
    g_nativeTelepromptSettings[1];
  g_nativeTelepromptSettingsUi.notice =
    g_nativeTechnicalNoticeSettings;
}

static void nativeTelepromptSettingsUiCloseColor()
{
  g_nativeTelepromptSettingsUi.colorOpen = false;
  g_nativeTelepromptSettingsUi.colorNotice = false;
  g_nativeTelepromptSettingsUi.colorKey.clear();
  g_nativeTelepromptSettingsUi.colorOriginal.clear();
  g_nativeTelepromptSettingsUi.colorHexInput.clear();
  g_nativeTelepromptSettingsUi.colorHexFocused = false;
  g_nativeTelepromptSettingsUi.colorHexReplaceOnType = false;
  g_nativeTelepromptSettingsUi.colorChannelDragging = -1;
  g_nativeTelepromptSettingsUi.colorChannelRect =
    RECT{0, 0, 0, 0};
}

static void nativeTelepromptSettingsUiOpenColor(
  bool notice,
  const std::string& key)
{
  std::string* value = notice
    ? nativeTelepromptSettingsUiNoticeColor(
        g_nativeTelepromptSettingsUi.notice, key)
    : nativeTelepromptSettingsUiTpColor(
        g_nativeTelepromptSettingsUi.teleprompt[
          g_nativeTelepromptSettingsUi.page == 1 ? 1 : 0],
        key);
  if (!value) return;
  int red = 255;
  int green = 234;
  int blue = 0;
  nativeTelepromptSettingsUiParseHex(
    *value, red, green, blue);
  g_nativeTelepromptSettingsUi.colorOpen = true;
  g_nativeTelepromptSettingsUi.colorNotice = notice;
  g_nativeTelepromptSettingsUi.colorKey = key;
  g_nativeTelepromptSettingsUi.colorOriginal = *value;
  g_nativeTelepromptSettingsUi.colorR = red;
  g_nativeTelepromptSettingsUi.colorG = green;
  g_nativeTelepromptSettingsUi.colorB = blue;
  g_nativeTelepromptSettingsUi.colorHexInput =
    nativeTelepromptSettingsUiHex(red, green, blue).substr(1);
  g_nativeTelepromptSettingsUi.colorHexFocused = false;
  g_nativeTelepromptSettingsUi.colorHexReplaceOnType = true;
  g_nativeTelepromptSettingsUi.dropdownKey.clear();
  g_nativeTelepromptSettingsUi.noticeEmojiFocused = false;
}

static void nativeTelepromptSettingsUiApplyColor()
{
  std::string* value =
    g_nativeTelepromptSettingsUi.colorNotice
      ? nativeTelepromptSettingsUiNoticeColor(
          g_nativeTelepromptSettingsUi.notice,
          g_nativeTelepromptSettingsUi.colorKey)
      : nativeTelepromptSettingsUiTpColor(
          g_nativeTelepromptSettingsUi.teleprompt[
            g_nativeTelepromptSettingsUi.page == 1 ? 1 : 0],
          g_nativeTelepromptSettingsUi.colorKey);
  if (!value) {
    nativeTelepromptSettingsUiCloseColor();
    return;
  }
  *value = nativeTelepromptSettingsUiHex(
    g_nativeTelepromptSettingsUi.colorR,
    g_nativeTelepromptSettingsUi.colorG,
    g_nativeTelepromptSettingsUi.colorB);
  if (g_nativeTelepromptSettingsUi.colorNotice) {
    nativeTelepromptSettingsUiCommitNotice();
  } else {
    nativeTelepromptSettingsUiCommitTp(
      g_nativeTelepromptSettingsUi.page == 1 ? 1 : 0);
  }
  nativeTelepromptSettingsUiCloseColor();
}

static const NativeTelepromptSettingsUiScaleField*
nativeTelepromptSettingsUiScaleDefinition(
  const std::string& key)
{
  for (const auto& field :
       kNativeTelepromptSettingsUiScales) {
    if (key == field.key) return &field;
  }
  return nullptr;
}

static void nativeTelepromptSettingsUiSetScaleFromPoint(int x)
{
  if (g_nativeTelepromptSettingsUi.page > 1) return;
  const auto* definition =
    nativeTelepromptSettingsUiScaleDefinition(
      g_nativeTelepromptSettingsUi.scaleKey);
  if (!definition) return;
  double* value = nativeTelepromptSettingsUiScale(
    g_nativeTelepromptSettingsUi.teleprompt[
      g_nativeTelepromptSettingsUi.page],
    g_nativeTelepromptSettingsUi.scaleKey);
  if (!value) return;
  const RECT& rect = g_nativeTelepromptSettingsUi.scaleRect;
  const int width = std::max(
    1, static_cast<int>(rect.right - rect.left));
  const double ratio = std::max(0.0, std::min(1.0,
    static_cast<double>(x - rect.left) / width));
  double next = definition->minimum +
    ratio * (definition->maximum - definition->minimum);
  next = std::round(next / definition->step) *
    definition->step;
  next = std::max(definition->minimum,
    std::min(definition->maximum, next));
  if (std::fabs(*value - next) < 0.0001) return;
  *value = next;
  g_nativeTelepromptSettingsUi.scaleDirty = true;
}

static void nativeTelepromptSettingsUiSetColorChannelFromPoint(
  int channel,
  int x)
{
  const RECT& rect =
    g_nativeTelepromptSettingsUi.colorChannelRect;
  const int width = std::max(
    1, static_cast<int>(rect.right - rect.left));
  const double ratio = std::max(0.0, std::min(1.0,
    static_cast<double>(x - rect.left) / width));
  const int value = std::max(
    0, std::min(255,
      static_cast<int>(std::round(ratio * 255.0))));
  if (channel == 0) g_nativeTelepromptSettingsUi.colorR = value;
  if (channel == 1) g_nativeTelepromptSettingsUi.colorG = value;
  if (channel == 2) g_nativeTelepromptSettingsUi.colorB = value;
  g_nativeTelepromptSettingsUi.colorHexInput =
    nativeTelepromptSettingsUiHex(
      g_nativeTelepromptSettingsUi.colorR,
      g_nativeTelepromptSettingsUi.colorG,
      g_nativeTelepromptSettingsUi.colorB).substr(1);
  g_nativeTelepromptSettingsUi.colorHexReplaceOnType = true;
}

static void nativeTelepromptSettingsUiApplyPreset(
  const std::string& preset)
{
  if (g_nativeTelepromptSettingsUi.page > 1) return;
  NativeTelepromptSettings& settings =
    g_nativeTelepromptSettingsUi.teleprompt[
      g_nativeTelepromptSettingsUi.page];
  const bool day = preset == "day";
  settings.preset = day ? "day" : "night";
  settings.textColor = day ? "#FFFFFF" : "#FFEA00";
  settings.textBoxColor = day ? "#FFFFFF" : "#FFEA00";
  settings.clockColor = day ? "#FFFFFF" : "#00FF55";
  settings.clockExpiredColor = day ? "#D60000" : "#FF3131";
  settings.clockBorderColor = day ? "#FFFFFF" : "#00FF55";
  settings.localClockColor = day ? "#FFFFFF" : "#00FF55";
  settings.borderColor = day ? "#FFFFFF" : "#00FF55";
  settings.songNameColor = day ? "#FFFFFF" : "#00FF55";
  settings.queueNameColor = day ? "#FFFFFF" : "#FFEA00";
  settings.progressColor = day ? "#FFFFFF" : "#FFEA00";
  nativeTelepromptSettingsUiCommitTp(
    g_nativeTelepromptSettingsUi.page);
}

static std::string nativeTelepromptSettingsUiFieldValue(
  const std::string& key)
{
  if (g_nativeTelepromptSettingsUi.page == 2) {
    if (key == "fontFamily") {
      return g_nativeTelepromptSettingsUi.notice.fontFamily;
    }
    return {};
  }
  std::string* value = nativeTelepromptSettingsUiTpChoice(
    g_nativeTelepromptSettingsUi.teleprompt[
      g_nativeTelepromptSettingsUi.page],
    key);
  return value ? *value : std::string();
}

static void nativeTelepromptSettingsUiSetChoice(
  const std::string& key,
  const std::string& value)
{
  if (g_nativeTelepromptSettingsUi.page == 2) {
    if (key == "fontFamily") {
      g_nativeTelepromptSettingsUi.notice.fontFamily = value;
      nativeTelepromptSettingsUiCommitNotice();
    }
    return;
  }
  std::string* target = nativeTelepromptSettingsUiTpChoice(
    g_nativeTelepromptSettingsUi.teleprompt[
      g_nativeTelepromptSettingsUi.page],
    key);
  if (!target) return;
  *target = value;
  nativeTelepromptSettingsUiCommitTp(
    g_nativeTelepromptSettingsUi.page);
}

static void nativeTelepromptSettingsUiDrawGroupTitle(
  HDC dc,
  const RECT& rect,
  const std::string& title)
{
  nativeAppActiveDrawText(dc, title, rect,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
    RGB(255, 224, 46), nativeUiSharedFont());
  nativeAppActiveFillRect(dc,
    RECT{rect.left, rect.bottom - 1,
      rect.right, rect.bottom},
    RGB(92, 101, 115));
}

static void nativeTelepromptSettingsUiDrawRowBase(
  HDC dc,
  const RECT& rect,
  const std::string& label)
{
  nativeAppActiveFillRoundRect(dc, rect,
    RGB(28, 34, 43), RGB(61, 70, 82), 3);
  const int controlWidth = std::max(
    120, static_cast<int>(
      (rect.right - rect.left) * 0.43));
  RECT labelRect{rect.left + 8, rect.top + 1,
    std::max(rect.left + 8, rect.right - controlWidth - 6),
    rect.bottom - 1};
  nativeAppActiveDrawText(dc, label, labelRect,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE |
      DT_END_ELLIPSIS | DT_NOPREFIX,
    RGB(226, 232, 240), nativeUiSharedFont());
}

static RECT nativeTelepromptSettingsUiControlRect(
  const RECT& row)
{
  const int width = std::max(
    120, static_cast<int>(
      (row.right - row.left) * 0.43));
  return RECT{row.right - width, row.top + 4,
    row.right - 5, row.bottom - 4};
}

static void nativeTelepromptSettingsUiDrawColorRow(
  HDC dc,
  const RECT& row,
  const NativeTelepromptSettingsUiField& field,
  bool notice)
{
  nativeTelepromptSettingsUiDrawRowBase(
    dc, row, field.label);
  std::string* value = notice
    ? nativeTelepromptSettingsUiNoticeColor(
        g_nativeTelepromptSettingsUi.notice, field.key)
    : nativeTelepromptSettingsUiTpColor(
        g_nativeTelepromptSettingsUi.teleprompt[
          g_nativeTelepromptSettingsUi.page],
        field.key);
  if (!value) return;
  const RECT control =
    nativeTelepromptSettingsUiControlRect(row);
  nativeAppActiveFillRoundRect(dc, control,
    RGB(45, 52, 63), RGB(94, 104, 118), 3);
  RECT swatch{control.right - 30, control.top + 3,
    control.right - 4, control.bottom - 3};
  nativeAppActiveFillRoundRect(dc, swatch,
    nativeTelepromptColor(*value, RGB(255, 234, 0)),
    RGB(220, 225, 230), 2);
  RECT valueRect{control.left + 6, control.top,
    swatch.left - 5, control.bottom};
  nativeAppActiveDrawText(dc, *value, valueRect,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE |
      DT_END_ELLIPSIS | DT_NOPREFIX,
    RGB(245, 247, 250), nativeUiSharedFont());
  nativeTelepromptSettingsUiAddHit(
    row, "color|" + std::string(field.key),
    true);
}

static void nativeTelepromptSettingsUiDrawToggleRow(
  HDC dc,
  const RECT& row,
  const NativeTelepromptSettingsUiField& field,
  bool notice)
{
  nativeTelepromptSettingsUiDrawRowBase(
    dc, row, field.label);
  bool* value = notice
    ? nativeTelepromptSettingsUiNoticeToggle(
        g_nativeTelepromptSettingsUi.notice, field.key)
    : nativeTelepromptSettingsUiTpToggle(
        g_nativeTelepromptSettingsUi.teleprompt[
          g_nativeTelepromptSettingsUi.page],
        field.key);
  if (!value) return;
  const RECT control =
    nativeTelepromptSettingsUiControlRect(row);
  const int boxSize = std::min(
    20, static_cast<int>(
      control.bottom - control.top));
  RECT box{control.right - boxSize,
    control.top + (control.bottom - control.top - boxSize) / 2,
    control.right,
    control.top + (control.bottom - control.top + boxSize) / 2};
  nativeAppActiveFillRoundRect(dc, box,
    *value ? RGB(28, 168, 92) : RGB(154, 42, 42),
    RGB(220, 225, 230), 2);
  nativeAppActiveDrawText(dc, *value ? "X" : "",
    box, DT_CENTER | DT_VCENTER | DT_SINGLELINE |
      DT_NOPREFIX,
    RGB(255, 255, 255), nativeUiSharedFont());
  RECT stateRect{control.left, control.top,
    box.left - 6, control.bottom};
  nativeAppActiveDrawText(dc,
    *value ? "Ativado" : "Desativado",
    stateRect,
    DT_RIGHT | DT_VCENTER | DT_SINGLELINE |
      DT_END_ELLIPSIS | DT_NOPREFIX,
    *value ? RGB(74, 222, 128) : RGB(248, 113, 113),
    nativeUiSharedFont());
  nativeTelepromptSettingsUiAddHit(
    row, "toggle|" + std::string(field.key),
    true);
}

static void nativeTelepromptSettingsUiDrawChoiceRow(
  HDC dc,
  const RECT& row,
  const NativeTelepromptSettingsUiField& field)
{
  nativeTelepromptSettingsUiDrawRowBase(
    dc, row, field.label);
  const RECT control =
    nativeTelepromptSettingsUiControlRect(row);
  nativeAppActiveFillRoundRect(dc, control,
    RGB(45, 52, 63), RGB(94, 104, 118), 3);
  const std::string value =
    nativeTelepromptSettingsUiFieldValue(field.key);
  RECT textRect{control.left + 6, control.top,
    control.right - 23, control.bottom};
  nativeAppActiveDrawText(dc,
    nativeTelepromptSettingsUiChoiceLabel(
      field.key, value),
    textRect,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE |
      DT_END_ELLIPSIS | DT_NOPREFIX,
    RGB(245, 247, 250), nativeUiSharedFont());
  RECT arrow{control.right - 20, control.top,
    control.right - 3, control.bottom};
  nativeAppActiveDrawText(dc, "v", arrow,
    DT_CENTER | DT_VCENTER | DT_SINGLELINE |
      DT_NOPREFIX,
    RGB(255, 224, 46), nativeUiSharedFont());
  nativeTelepromptSettingsUiAddHit(
    row, "select|" + std::string(field.key),
    true);
}

static void nativeTelepromptSettingsUiDrawScaleRow(
  HDC dc,
  const RECT& row,
  const NativeTelepromptSettingsUiScaleField& field)
{
  nativeTelepromptSettingsUiDrawRowBase(
    dc, row, field.label);
  double* value = nativeTelepromptSettingsUiScale(
    g_nativeTelepromptSettingsUi.teleprompt[
      g_nativeTelepromptSettingsUi.page],
    field.key);
  if (!value) return;
  RECT control =
    nativeTelepromptSettingsUiControlRect(row);
  const int valueWidth = 43;
  RECT track{control.left + 4,
    control.top + (control.bottom - control.top) / 2 - 2,
    control.right - valueWidth - 6,
    control.top + (control.bottom - control.top) / 2 + 3};
  nativeAppActiveFillRoundRect(dc, track,
    RGB(65, 73, 86), RGB(101, 112, 127), 2);
  const double ratio = std::max(0.0, std::min(1.0,
    (*value - field.minimum) /
      std::max(0.001, field.maximum - field.minimum)));
  const int fillRight = track.left +
    static_cast<int>(std::round(
      ratio * (track.right - track.left)));
  nativeAppActiveFillRoundRect(dc,
    RECT{track.left, track.top,
      static_cast<LONG>(std::max(
        static_cast<int>(track.left) + 1, fillRight)),
      track.bottom},
    RGB(61, 133, 209), RGB(61, 133, 209), 2);
  RECT knob{fillRight - 4, track.top - 4,
    fillRight + 5, track.bottom + 4};
  nativeAppActiveFillRoundRect(dc, knob,
    RGB(255, 224, 46), RGB(255, 255, 255), 3);
  RECT valueRect{control.right - valueWidth,
    control.top, control.right, control.bottom};
  nativeAppActiveDrawText(dc,
    std::to_string(
      static_cast<int>(std::round(*value * 100.0))) + "%",
    valueRect,
    DT_RIGHT | DT_VCENTER | DT_SINGLELINE |
      DT_NOPREFIX,
    RGB(245, 247, 250), nativeUiSharedFont());
  nativeTelepromptSettingsUiAddHit(
    track, "scale|" + std::string(field.key),
    true);
}

template <typename FieldType, size_t Count, typename DrawFunction>
static int nativeTelepromptSettingsUiDrawGrid(
  HDC dc,
  int top,
  const FieldType (&fields)[Count],
  DrawFunction draw)
{
  const RECT& view = g_nativeTelepromptSettingsUi.viewport;
  const int availableWidth = std::max(
    220, static_cast<int>(view.right - view.left) - 12);
  const int columns = availableWidth >= 690 ? 2 : 1;
  const int gap = 7;
  const int rowHeight = 34;
  const int columnWidth =
    (availableWidth - (columns - 1) * gap) / columns;
  for (size_t index = 0; index < Count; ++index) {
    const int column = static_cast<int>(index) % columns;
    const int rowIndex = static_cast<int>(index) / columns;
    RECT row{
      view.left + 6 + column * (columnWidth + gap),
      top + rowIndex * (rowHeight + 5),
      view.left + 6 + column * (columnWidth + gap) +
        columnWidth,
      top + rowIndex * (rowHeight + 5) + rowHeight
    };
    if (nativeTelepromptSettingsUiRectVisible(row)) {
      draw(row, fields[index]);
    }
  }
  const int rows = static_cast<int>(
    (Count + static_cast<size_t>(columns) - 1) /
    static_cast<size_t>(columns));
  return top + rows * (rowHeight + 5);
}

static void nativeTelepromptSettingsUiDrawDropdown(
  HDC dc,
  const RECT& client)
{
  if (g_nativeTelepromptSettingsUi.dropdownKey.empty()) return;
  const auto options = nativeTelepromptSettingsUiChoiceOptions(
    g_nativeTelepromptSettingsUi.dropdownKey);
  if (options.empty()) return;
  const int rowHeight = 27;
  const int width = std::max(
    190, static_cast<int>(
      g_nativeTelepromptSettingsUi.dropdownAnchor.right -
      g_nativeTelepromptSettingsUi.dropdownAnchor.left));
  const int height =
    static_cast<int>(options.size()) * rowHeight + 4;
  int left = std::max(static_cast<int>(client.left) + 8,
    static_cast<int>(
      g_nativeTelepromptSettingsUi.dropdownAnchor.right - width));
  left = std::min(left,
    std::max(static_cast<int>(client.left) + 8,
      static_cast<int>(client.right - width - 8)));
  int top = g_nativeTelepromptSettingsUi.dropdownAnchor.bottom + 2;
  if (top + height > client.bottom - 42) {
    top = g_nativeTelepromptSettingsUi.dropdownAnchor.top -
      height - 2;
  }
  top = std::max(static_cast<int>(client.top) + 42, top);
  RECT panel{left, top, left + width, top + height};
  nativeAppActiveFillRoundRect(dc, panel,
    RGB(15, 20, 28), RGB(255, 224, 46), 4);
  const std::string selected =
    nativeTelepromptSettingsUiFieldValue(
      g_nativeTelepromptSettingsUi.dropdownKey);
  for (size_t index = 0; index < options.size(); ++index) {
    RECT row{panel.left + 2,
      panel.top + 2 + static_cast<int>(index) * rowHeight,
      panel.right - 2,
      panel.top + 2 +
        static_cast<int>(index + 1) * rowHeight};
    const bool active = options[index].first == selected;
    nativeAppActiveFillRect(dc, row,
      active ? RGB(28, 168, 92) : RGB(38, 45, 56));
    nativeAppActiveDrawText(dc, options[index].second,
      RECT{row.left + 7, row.top,
        row.right - 5, row.bottom},
      DT_LEFT | DT_VCENTER | DT_SINGLELINE |
        DT_END_ELLIPSIS | DT_NOPREFIX,
      active ? RGB(10, 18, 14) : RGB(241, 245, 249),
      nativeUiSharedFont());
    nativeTelepromptSettingsUiAddHit(
      row, "option|" + options[index].first);
  }
}

static void nativeTelepromptSettingsUiDrawColorEditor(
  HDC dc,
  const RECT& client)
{
  if (!g_nativeTelepromptSettingsUi.colorOpen) return;
  nativeAppActiveFillRect(dc, client, RGB(5, 8, 12));
  const int width = std::min(
    560, std::max(320,
      static_cast<int>(client.right - client.left) - 30));
  const int height = std::min(
    455, std::max(390,
      static_cast<int>(client.bottom - client.top) - 30));
  const int left = client.left +
    (client.right - client.left - width) / 2;
  const int top = client.top +
    (client.bottom - client.top - height) / 2;
  RECT panel{left, top, left + width, top + height};
  nativeAppActiveFillRoundRect(dc, panel,
    RGB(20, 26, 35), RGB(255, 224, 46), 6);
  nativeAppActiveDrawText(dc, "Seletor de cor RGB",
    RECT{panel.left + 16, panel.top + 10,
      panel.right - 16, panel.top + 40},
    DT_LEFT | DT_VCENTER | DT_SINGLELINE |
      DT_NOPREFIX,
    RGB(255, 224, 46), nativeUiPopupFont());

  const char* channelNames[] = {
    "Vermelho", "Verde", "Azul"
  };
  const COLORREF channelColors[] = {
    RGB(235, 70, 70), RGB(52, 200, 98), RGB(62, 126, 235)
  };
  const int channelValues[] = {
    g_nativeTelepromptSettingsUi.colorR,
    g_nativeTelepromptSettingsUi.colorG,
    g_nativeTelepromptSettingsUi.colorB
  };
  int channelTop = panel.top + 52;
  for (int channel = 0; channel < 3; ++channel) {
    RECT label{panel.left + 18, channelTop,
      panel.left + 104, channelTop + 28};
    nativeAppActiveDrawText(dc, channelNames[channel],
      label, DT_LEFT | DT_VCENTER | DT_SINGLELINE |
        DT_NOPREFIX,
      RGB(230, 235, 240), nativeUiSharedFont());
    RECT track{panel.left + 110, channelTop + 10,
      panel.right - 74, channelTop + 16};
    nativeAppActiveFillRoundRect(dc, track,
      RGB(63, 71, 84), RGB(102, 112, 127), 2);
    const int fillRight = track.left +
      static_cast<int>(std::round(
        (track.right - track.left) *
        channelValues[channel] / 255.0));
    nativeAppActiveFillRoundRect(dc,
      RECT{track.left, track.top,
        static_cast<LONG>(std::max(
          static_cast<int>(track.left) + 1, fillRight)),
        track.bottom},
      channelColors[channel], channelColors[channel], 2);
    nativeAppActiveFillRoundRect(dc,
      RECT{fillRight - 5, track.top - 5,
        fillRight + 6, track.bottom + 5},
      RGB(255, 255, 255), channelColors[channel], 3);
    RECT value{panel.right - 64, channelTop,
      panel.right - 18, channelTop + 28};
    nativeAppActiveFillRoundRect(dc, value,
      RGB(42, 49, 60), RGB(91, 102, 116), 3);
    nativeAppActiveDrawText(dc,
      std::to_string(channelValues[channel]),
      value, DT_CENTER | DT_VCENTER | DT_SINGLELINE |
        DT_NOPREFIX,
      RGB(245, 247, 250), nativeUiSharedFont());
    nativeTelepromptSettingsUiAddHit(
      RECT{track.left, track.top - 8,
        track.right, track.bottom + 8},
      "color_channel|" + std::to_string(channel));
    channelTop += 43;
  }

  RECT hexLabel{panel.left + 18, channelTop + 4,
    panel.left + 100, channelTop + 34};
  nativeAppActiveDrawText(dc, "Hexadecimal",
    hexLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE |
      DT_NOPREFIX,
    RGB(230, 235, 240), nativeUiSharedFont());
  RECT hexBox{panel.left + 110, channelTop + 4,
    panel.left + 250, channelTop + 34};
  nativeAppActiveFillRoundRect(dc, hexBox,
    RGB(42, 49, 60),
    g_nativeTelepromptSettingsUi.colorHexFocused
      ? RGB(255, 224, 46) : RGB(91, 102, 116), 3);
  nativeAppActiveDrawText(dc,
    "#" + g_nativeTelepromptSettingsUi.colorHexInput,
    RECT{hexBox.left + 8, hexBox.top,
      hexBox.right - 6, hexBox.bottom},
    DT_LEFT | DT_VCENTER | DT_SINGLELINE |
      DT_NOPREFIX,
    RGB(245, 247, 250), nativeUiSharedFont());
  nativeTelepromptSettingsUiAddHit(hexBox, "color_hex");
  RECT preview{panel.right - 118, channelTop + 2,
    panel.right - 18, channelTop + 54};
  nativeAppActiveFillRoundRect(dc, preview,
    RGB(g_nativeTelepromptSettingsUi.colorR,
      g_nativeTelepromptSettingsUi.colorG,
      g_nativeTelepromptSettingsUi.colorB),
    RGB(235, 238, 242), 4);

  const char* palette[] = {
    "#FFFFFF", "#000000", "#FFEA00", "#00FF55", "#FF3131",
    "#2F80ED", "#9B51E0", "#FF7A00", "#00C2D1", "#FF4FA3"
  };
  const int paletteTop = channelTop + 68;
  const int paletteGap = 6;
  const int paletteWidth =
    (width - 36 - 4 * paletteGap) / 5;
  for (int index = 0; index < 10; ++index) {
    const int column = index % 5;
    const int row = index / 5;
    RECT swatch{
      panel.left + 18 + column * (paletteWidth + paletteGap),
      paletteTop + row * 37,
      panel.left + 18 + column * (paletteWidth + paletteGap) +
        paletteWidth,
      paletteTop + row * 37 + 31
    };
    nativeAppActiveFillRoundRect(dc, swatch,
      nativeTelepromptColor(palette[index], RGB(255, 234, 0)),
      RGB(220, 225, 230), 3);
    nativeTelepromptSettingsUiAddHit(
      swatch, "color_palette|" +
        std::string(palette[index]));
  }

  const int buttonTop = panel.bottom - 42;
  const int buttonGap = 8;
  const int buttonWidth = (width - 36 - buttonGap) / 2;
  RECT cancel{panel.left + 18, buttonTop,
    panel.left + 18 + buttonWidth, panel.bottom - 12};
  RECT apply{cancel.right + buttonGap, buttonTop,
    panel.right - 18, panel.bottom - 12};
  nativeTelepromptSettingsUiDrawButton(
    dc, cancel, "Cancelar", false);
  nativeTelepromptSettingsUiDrawButton(
    dc, apply, "Aplicar cor", true);
  nativeTelepromptSettingsUiAddHit(
    cancel, "color_cancel");
  nativeTelepromptSettingsUiAddHit(
    apply, "color_apply");
}

static void nativeTelepromptSettingsUiPaint(HWND hwnd)
{
  PAINTSTRUCT paint{};
  HDC paintDc = BeginPaint(hwnd, &paint);
  if (!paintDc) return;
  RECT client{0, 0, 0, 0};
  GetClientRect(hwnd, &client);
  const int width = std::max(
    1, static_cast<int>(client.right - client.left));
  const int height = std::max(
    1, static_cast<int>(client.bottom - client.top));
  HDC dc = nativeTelepromptSettingsUiBuffer(
    paintDc, width, height);
  if (!dc) dc = paintDc;
  const bool previousModernButtonDesign =
    g_nativeUiModernButtonDesign;
  g_nativeUiModernButtonDesign = false;
  g_nativeTelepromptSettingsUi.hits.clear();
  nativeAppActiveFillRect(dc, client, RGB(8, 13, 21));
  nativeAppActiveStrokeRoundRect(dc,
    RECT{client.left + 1, client.top + 1,
      client.right - 1, client.bottom - 1},
    RGB(255, 128, 0), 5);

  const int margin = 10;
  const int tabGap = 6;
  const int tabHeight = 30;
  const int tabWidth =
    std::max(90, (width - margin * 2 - tabGap * 2) / 3);
  const char* tabNames[] = {
    "Config Teleprompt 1",
    "Config Teleprompt 2",
    "Config Recados"
  };
  for (int index = 0; index < 3; ++index) {
    RECT tab{margin + index * (tabWidth + tabGap),
      margin,
      margin + index * (tabWidth + tabGap) + tabWidth,
      margin + tabHeight};
    if (index == 2) tab.right = client.right - margin;
    nativeTelepromptSettingsUiDrawButton(dc, tab,
      tabNames[index],
      g_nativeTelepromptSettingsUi.page == index);
    nativeTelepromptSettingsUiAddHit(
      tab, "tab|" + std::to_string(index));
  }

  int bodyTop = margin + tabHeight + 8;
  if (g_nativeTelepromptSettingsUi.page < 2) {
    NativeTelepromptSettings& settings =
      g_nativeTelepromptSettingsUi.teleprompt[
        g_nativeTelepromptSettingsUi.page];
    const int buttonWidth = std::max(
      85, std::min(150, (width - margin * 2 - 12) / 3));
    RECT night{margin, bodyTop,
      margin + buttonWidth, bodyTop + 28};
    RECT day{night.right + 6, bodyTop,
      night.right + 6 + buttonWidth, bodyTop + 28};
    RECT clear{day.right + 6, bodyTop,
      std::min(client.right - margin,
        day.right + 6 + std::max(buttonWidth, 120)),
      bodyTop + 28};
    nativeTelepromptSettingsUiDrawButton(
      dc, night, "Noite", settings.preset != "day");
    nativeTelepromptSettingsUiDrawButton(
      dc, day, "Dia", settings.preset == "day",
      true, RGB(225, 225, 225));
    nativeTelepromptSettingsUiDrawButton(
      dc, clear,
      settings.clearMode ? "Modo Clear ON" : "Modo Clear",
      settings.clearMode,
      true, RGB(184, 51, 51));
    nativeTelepromptSettingsUiAddHit(night, "preset|night");
    nativeTelepromptSettingsUiAddHit(day, "preset|day");
    nativeTelepromptSettingsUiAddHit(clear, "clear");
    bodyTop += 36;
  }

  const int footerHeight = 38;
  g_nativeTelepromptSettingsUi.viewport =
    RECT{margin, bodyTop, client.right - margin - 10,
      client.bottom - footerHeight};
  g_nativeTelepromptSettingsUi.scrollTrack =
    RECT{client.right - margin - 7, bodyTop,
      client.right - margin - 1,
      client.bottom - footerHeight};

  const NativeUiClipState clip =
    nativeUiBeginClipRect(
      dc, g_nativeTelepromptSettingsUi.viewport);
  int contentTop =
    g_nativeTelepromptSettingsUi.viewport.top + 7 -
    g_nativeTelepromptSettingsUi.scroll;
  auto drawTitle = [&](const std::string& title) {
    RECT titleRect{
      g_nativeTelepromptSettingsUi.viewport.left + 6,
      contentTop,
      g_nativeTelepromptSettingsUi.viewport.right - 6,
      contentTop + 23};
    if (nativeTelepromptSettingsUiRectVisible(titleRect)) {
      nativeTelepromptSettingsUiDrawGroupTitle(
        dc, titleRect, title);
    }
    contentTop += 28;
  };

  if (g_nativeTelepromptSettingsUi.page < 2) {
    drawTitle("Cores");
    contentTop = nativeTelepromptSettingsUiDrawGrid(
      dc, contentTop,
      kNativeTelepromptSettingsUiColors,
      [&](const RECT& row,
          const NativeTelepromptSettingsUiField& field) {
        nativeTelepromptSettingsUiDrawColorRow(
          dc, row, field, false);
      }) + 8;

    drawTitle("Escalas");
    contentTop = nativeTelepromptSettingsUiDrawGrid(
      dc, contentTop,
      kNativeTelepromptSettingsUiScales,
      [&](const RECT& row,
          const NativeTelepromptSettingsUiScaleField& field) {
        nativeTelepromptSettingsUiDrawScaleRow(
          dc, row, field);
      }) + 8;

    drawTitle("Fontes e posicao");
    contentTop = nativeTelepromptSettingsUiDrawGrid(
      dc, contentTop,
      kNativeTelepromptSettingsUiChoices,
      [&](const RECT& row,
          const NativeTelepromptSettingsUiField& field) {
        nativeTelepromptSettingsUiDrawChoiceRow(
          dc, row, field);
      }) + 8;

    drawTitle("Mostrar");
    contentTop = nativeTelepromptSettingsUiDrawGrid(
      dc, contentTop,
      kNativeTelepromptSettingsUiToggles,
      [&](const RECT& row,
          const NativeTelepromptSettingsUiField& field) {
        nativeTelepromptSettingsUiDrawToggleRow(
          dc, row, field, false);
      }) + 8;
  } else {
    drawTitle("Cores e fonte");
    contentTop = nativeTelepromptSettingsUiDrawGrid(
      dc, contentTop,
      kNativeTelepromptSettingsUiNoticeColors,
      [&](const RECT& row,
          const NativeTelepromptSettingsUiField& field) {
        nativeTelepromptSettingsUiDrawColorRow(
          dc, row, field, true);
      }) + 5;
    const NativeTelepromptSettingsUiField fontFields[] = {
      {"fontFamily", "Fonte do recado"}
    };
    contentTop = nativeTelepromptSettingsUiDrawGrid(
      dc, contentTop, fontFields,
      [&](const RECT& row,
          const NativeTelepromptSettingsUiField& field) {
        nativeTelepromptSettingsUiDrawChoiceRow(
          dc, row, field);
      }) + 8;

    drawTitle("Exibicao");
    contentTop = nativeTelepromptSettingsUiDrawGrid(
      dc, contentTop,
      kNativeTelepromptSettingsUiNoticeToggles,
      [&](const RECT& row,
          const NativeTelepromptSettingsUiField& field) {
        nativeTelepromptSettingsUiDrawToggleRow(
          dc, row, field, true);
      }) + 5;

    RECT emojiRow{
      g_nativeTelepromptSettingsUi.viewport.left + 6,
      contentTop,
      g_nativeTelepromptSettingsUi.viewport.right - 6,
      contentTop + 34};
    if (nativeTelepromptSettingsUiRectVisible(emojiRow)) {
      nativeTelepromptSettingsUiDrawRowBase(
        dc, emojiRow, "Simbolo");
      RECT emojiControl =
        nativeTelepromptSettingsUiControlRect(emojiRow);
      nativeAppActiveFillRoundRect(dc, emojiControl,
        RGB(45, 52, 63),
        g_nativeTelepromptSettingsUi.noticeEmojiFocused
          ? RGB(255, 224, 46) : RGB(94, 104, 118), 3);
      nativeAppActiveDrawText(dc,
        g_nativeTelepromptSettingsUi.notice.emoji,
        RECT{emojiControl.left + 7, emojiControl.top,
          emojiControl.right - 6, emojiControl.bottom},
        DT_LEFT | DT_VCENTER | DT_SINGLELINE |
          DT_END_ELLIPSIS | DT_NOPREFIX,
        RGB(245, 247, 250), nativeUiSharedFont());
      nativeTelepromptSettingsUiAddHit(
        emojiRow, "notice_emoji", true);
    }
    contentTop += 42;
  }
  nativeUiEndClipRect(dc, clip);

  const int viewportHeight = std::max(1,
    static_cast<int>(
      g_nativeTelepromptSettingsUi.viewport.bottom -
      g_nativeTelepromptSettingsUi.viewport.top));
  const int contentHeight = std::max(
    viewportHeight,
    contentTop + g_nativeTelepromptSettingsUi.scroll -
      static_cast<int>(
        g_nativeTelepromptSettingsUi.viewport.top) + 4);
  g_nativeTelepromptSettingsUi.maxScroll =
    std::max(0, contentHeight - viewportHeight);
  g_nativeTelepromptSettingsUi.scroll = std::max(
    0, std::min(
      g_nativeTelepromptSettingsUi.maxScroll,
      g_nativeTelepromptSettingsUi.scroll));

  nativeAppActiveFillRoundRect(dc,
    g_nativeTelepromptSettingsUi.scrollTrack,
    RGB(30, 36, 45), RGB(55, 63, 74), 2);
  const int trackHeight = std::max(1,
    static_cast<int>(
      g_nativeTelepromptSettingsUi.scrollTrack.bottom -
      g_nativeTelepromptSettingsUi.scrollTrack.top));
  const int thumbHeight = g_nativeTelepromptSettingsUi.maxScroll > 0
    ? std::max(30,
        static_cast<int>(std::round(
          trackHeight *
          (static_cast<double>(viewportHeight) /
           std::max(viewportHeight, contentHeight)))))
    : trackHeight;
  const int thumbTravel = std::max(0, trackHeight - thumbHeight);
  const int thumbTop = g_nativeTelepromptSettingsUi.scrollTrack.top +
    (g_nativeTelepromptSettingsUi.maxScroll > 0
      ? static_cast<int>(std::round(
          thumbTravel *
          (static_cast<double>(
            g_nativeTelepromptSettingsUi.scroll) /
           g_nativeTelepromptSettingsUi.maxScroll)))
      : 0);
  g_nativeTelepromptSettingsUi.scrollThumb =
    RECT{g_nativeTelepromptSettingsUi.scrollTrack.left,
      thumbTop,
      g_nativeTelepromptSettingsUi.scrollTrack.right,
      thumbTop + thumbHeight};
  nativeAppActiveFillRoundRect(dc,
    g_nativeTelepromptSettingsUi.scrollThumb,
    RGB(61, 190, 199), RGB(180, 235, 240), 2);
  nativeTelepromptSettingsUiAddHit(
    g_nativeTelepromptSettingsUi.scrollTrack,
    "scrollbar");

  RECT status{margin, client.bottom - footerHeight + 4,
    client.right - 110, client.bottom - 6};
  nativeAppActiveDrawText(dc,
    g_nativeTelepromptSettingsUi.status,
    status,
    DT_LEFT | DT_VCENTER | DT_SINGLELINE |
      DT_END_ELLIPSIS | DT_NOPREFIX,
    RGB(74, 222, 128), nativeUiSharedFont());
  RECT close{client.right - 98,
    client.bottom - footerHeight + 5,
    client.right - margin,
    client.bottom - 7};
  nativeTelepromptSettingsUiDrawButton(
    dc, close, "Fechar", false);
  nativeTelepromptSettingsUiAddHit(close, "close");

  if (!g_nativeTelepromptSettingsUi.colorOpen) {
    nativeTelepromptSettingsUiDrawDropdown(dc, client);
  }
  nativeTelepromptSettingsUiDrawColorEditor(dc, client);

  if (dc != paintDc) {
    BitBlt(paintDc, 0, 0, width, height,
      dc, 0, 0, SRCCOPY);
  }
  g_nativeUiModernButtonDesign =
    previousModernButtonDesign;
  EndPaint(hwnd, &paint);
}

static bool nativeTelepromptSettingsUiHandleAction(
  HWND hwnd,
  const NativeTelepromptSettingsUiHit& hit,
  const POINT& point)
{
  const std::vector<std::string> parts =
    nativeSplit(hit.action, '|');
  const std::string action =
    parts.empty() ? std::string() : parts[0];
  const std::string argument =
    parts.size() > 1 ? parts[1] : std::string();

  if (g_nativeTelepromptSettingsUi.colorOpen) {
    if (action == "color_cancel") {
      nativeTelepromptSettingsUiCloseColor();
      return true;
    }
    if (action == "color_apply") {
      nativeTelepromptSettingsUiApplyColor();
      return true;
    }
    if (action == "color_hex") {
      g_nativeTelepromptSettingsUi.colorHexFocused = true;
      g_nativeTelepromptSettingsUi.colorHexReplaceOnType = true;
      return true;
    }
    if (action == "color_palette") {
      int red = 0;
      int green = 0;
      int blue = 0;
      if (nativeTelepromptSettingsUiParseHex(
            argument, red, green, blue)) {
        g_nativeTelepromptSettingsUi.colorR = red;
        g_nativeTelepromptSettingsUi.colorG = green;
        g_nativeTelepromptSettingsUi.colorB = blue;
        g_nativeTelepromptSettingsUi.colorHexInput =
          nativeTelepromptSettingsUiHex(
            red, green, blue).substr(1);
        g_nativeTelepromptSettingsUi.colorHexReplaceOnType = true;
      }
      return true;
    }
    if (action == "color_channel") {
      const int channel = std::max(
        0, std::min(2, std::atoi(argument.c_str())));
      g_nativeTelepromptSettingsUi.colorChannelDragging =
        channel;
      g_nativeTelepromptSettingsUi.colorChannelRect =
        hit.rect;
      SetCapture(hwnd);
      nativeTelepromptSettingsUiSetColorChannelFromPoint(
        channel, point.x);
      return true;
    }
    return true;
  }

  if (!g_nativeTelepromptSettingsUi.dropdownKey.empty()) {
    if (action == "option") {
      nativeTelepromptSettingsUiSetChoice(
        g_nativeTelepromptSettingsUi.dropdownKey,
        argument);
      g_nativeTelepromptSettingsUi.dropdownKey.clear();
      return true;
    }
    g_nativeTelepromptSettingsUi.dropdownKey.clear();
    return true;
  }

  if (action == "tab") {
    g_nativeTelepromptSettingsUi.page = std::max(
      0, std::min(2, std::atoi(argument.c_str())));
    g_nativeTelepromptSettingsUi.scroll = 0;
    g_nativeTelepromptSettingsUi.noticeEmojiFocused = false;
    return true;
  }
  if (action == "preset") {
    nativeTelepromptSettingsUiApplyPreset(argument);
    return true;
  }
  if (action == "clear" &&
      g_nativeTelepromptSettingsUi.page < 2) {
    NativeTelepromptSettings& settings =
      g_nativeTelepromptSettingsUi.teleprompt[
        g_nativeTelepromptSettingsUi.page];
    settings.clearMode = !settings.clearMode;
    nativeTelepromptSettingsUiCommitTp(
      g_nativeTelepromptSettingsUi.page);
    return true;
  }
  if (action == "color") {
    nativeTelepromptSettingsUiOpenColor(
      g_nativeTelepromptSettingsUi.page == 2,
      argument);
    return true;
  }
  if (action == "toggle") {
    if (g_nativeTelepromptSettingsUi.page == 2) {
      bool* value = nativeTelepromptSettingsUiNoticeToggle(
        g_nativeTelepromptSettingsUi.notice, argument);
      if (value) {
        *value = !*value;
        nativeTelepromptSettingsUiCommitNotice();
      }
    } else {
      bool* value = nativeTelepromptSettingsUiTpToggle(
        g_nativeTelepromptSettingsUi.teleprompt[
          g_nativeTelepromptSettingsUi.page],
        argument);
      if (value) {
        *value = !*value;
        nativeTelepromptSettingsUiCommitTp(
          g_nativeTelepromptSettingsUi.page);
      }
    }
    return true;
  }
  if (action == "select") {
    g_nativeTelepromptSettingsUi.dropdownKey = argument;
    g_nativeTelepromptSettingsUi.dropdownAnchor = hit.rect;
    g_nativeTelepromptSettingsUi.noticeEmojiFocused = false;
    return true;
  }
  if (action == "scale" &&
      g_nativeTelepromptSettingsUi.page < 2) {
    g_nativeTelepromptSettingsUi.scaleDragging = true;
    g_nativeTelepromptSettingsUi.scaleDirty = false;
    g_nativeTelepromptSettingsUi.scaleKey = argument;
    g_nativeTelepromptSettingsUi.scaleRect = hit.rect;
    SetCapture(hwnd);
    nativeTelepromptSettingsUiSetScaleFromPoint(point.x);
    return true;
  }
  if (action == "notice_emoji") {
    g_nativeTelepromptSettingsUi.noticeEmojiFocused = true;
    return true;
  }
  if (action == "scrollbar") {
    if (PtInRect(
          &g_nativeTelepromptSettingsUi.scrollThumb,
          point)) {
      g_nativeTelepromptSettingsUi.scrollDragging = true;
      g_nativeTelepromptSettingsUi.scrollDragStartY = point.y;
      g_nativeTelepromptSettingsUi.scrollDragStartValue =
        g_nativeTelepromptSettingsUi.scroll;
      SetCapture(hwnd);
    } else if (g_nativeTelepromptSettingsUi.maxScroll > 0) {
      const int trackHeight = std::max(
        1, static_cast<int>(
          g_nativeTelepromptSettingsUi.scrollTrack.bottom -
          g_nativeTelepromptSettingsUi.scrollTrack.top));
      const double ratio = std::max(0.0, std::min(1.0,
        static_cast<double>(
          point.y -
          g_nativeTelepromptSettingsUi.scrollTrack.top) /
        trackHeight));
      g_nativeTelepromptSettingsUi.scroll =
        static_cast<int>(std::round(
          ratio *
          g_nativeTelepromptSettingsUi.maxScroll));
    }
    return true;
  }
  if (action == "close") {
    DestroyWindow(hwnd);
    return true;
  }
  return false;
}

static void nativeTelepromptSettingsUiHandleMouseDown(
  HWND hwnd,
  const POINT& point)
{
  if (GetFocus() != hwnd) SetFocus(hwnd);
  for (auto iterator =
         g_nativeTelepromptSettingsUi.hits.rbegin();
       iterator != g_nativeTelepromptSettingsUi.hits.rend();
       ++iterator) {
    if (!PtInRect(&iterator->rect, point)) continue;
    nativeTelepromptSettingsUiHandleAction(
      hwnd, *iterator, point);
    InvalidateRect(hwnd, nullptr, FALSE);
    return;
  }
  if (g_nativeTelepromptSettingsUi.colorOpen) {
    g_nativeTelepromptSettingsUi.colorHexFocused = false;
  } else {
    g_nativeTelepromptSettingsUi.dropdownKey.clear();
    g_nativeTelepromptSettingsUi.noticeEmojiFocused = false;
  }
  InvalidateRect(hwnd, nullptr, FALSE);
}

static void nativeTelepromptSettingsUiHandleMouseMove(
  HWND hwnd,
  const POINT& point)
{
  if (g_nativeTelepromptSettingsUi.scaleDragging) {
    nativeTelepromptSettingsUiSetScaleFromPoint(point.x);
    InvalidateRect(hwnd, nullptr, FALSE);
    return;
  }
  if (g_nativeTelepromptSettingsUi.colorChannelDragging >= 0) {
    nativeTelepromptSettingsUiSetColorChannelFromPoint(
      g_nativeTelepromptSettingsUi.colorChannelDragging,
      point.x);
    InvalidateRect(hwnd, nullptr, FALSE);
    return;
  }
  if (g_nativeTelepromptSettingsUi.scrollDragging) {
    const int trackHeight = std::max(
      1, static_cast<int>(
        g_nativeTelepromptSettingsUi.scrollTrack.bottom -
        g_nativeTelepromptSettingsUi.scrollTrack.top));
    const int thumbHeight = std::max(
      1, static_cast<int>(
        g_nativeTelepromptSettingsUi.scrollThumb.bottom -
        g_nativeTelepromptSettingsUi.scrollThumb.top));
    const int travel = std::max(1, trackHeight - thumbHeight);
    const int delta = point.y -
      g_nativeTelepromptSettingsUi.scrollDragStartY;
    g_nativeTelepromptSettingsUi.scroll = std::max(
      0, std::min(
        g_nativeTelepromptSettingsUi.maxScroll,
        g_nativeTelepromptSettingsUi.scrollDragStartValue +
          static_cast<int>(std::round(
            delta *
            static_cast<double>(
              g_nativeTelepromptSettingsUi.maxScroll) /
            travel))));
    InvalidateRect(hwnd, nullptr, FALSE);
  }
}

static void nativeTelepromptSettingsUiHandleMouseUp(HWND hwnd)
{
  if (g_nativeTelepromptSettingsUi.scaleDragging) {
    if (g_nativeTelepromptSettingsUi.scaleDirty) {
      nativeTelepromptSettingsUiCommitTp(
        g_nativeTelepromptSettingsUi.page == 1 ? 1 : 0);
    }
    g_nativeTelepromptSettingsUi.scaleDragging = false;
    g_nativeTelepromptSettingsUi.scaleDirty = false;
    g_nativeTelepromptSettingsUi.scaleKey.clear();
  }
  g_nativeTelepromptSettingsUi.colorChannelDragging = -1;
  g_nativeTelepromptSettingsUi.scrollDragging = false;
  if (GetCapture() == hwnd) ReleaseCapture();
  InvalidateRect(hwnd, nullptr, FALSE);
}

static void nativeTelepromptSettingsUiSaveWindowRect(HWND hwnd)
{
  if (!hwnd || !IsWindow(hwnd)) return;
  RECT rect{0, 0, 0, 0};
  if (!GetWindowRect(hwnd, &rect)) return;
  nativeAppActiveWriteWindowInt(
    "TP_SETTINGS_WINDOW_X_V1", rect.left);
  nativeAppActiveWriteWindowInt(
    "TP_SETTINGS_WINDOW_Y_V1", rect.top);
  nativeAppActiveWriteWindowInt(
    "TP_SETTINGS_WINDOW_W_V1",
    std::max(600, static_cast<int>(rect.right - rect.left)));
  nativeAppActiveWriteWindowInt(
    "TP_SETTINGS_WINDOW_H_V1",
    std::max(480, static_cast<int>(rect.bottom - rect.top)));
}

static LRESULT CALLBACK nativeTelepromptSettingsUiWndProc(
  HWND hwnd,
  UINT message,
  WPARAM wParam,
  LPARAM lParam)
{
  switch (message) {
    case WM_CREATE:
    case WM_INITDIALOG:
      return 0;
    case WM_NCHITTEST:
      return HTCLIENT;
#ifdef _WIN32
    case WM_GETDLGCODE:
      return DLGC_WANTALLKEYS | DLGC_WANTCHARS |
        DLGC_WANTARROWS | DLGC_WANTTAB;
#endif
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT:
      nativeTelepromptSettingsUiPaint(hwnd);
      return 0;
    case WM_SIZE:
      nativeTelepromptSettingsUiReleaseBackBuffer();
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_GETMINMAXINFO:
      if (lParam) {
        auto* info =
          reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 600;
        info->ptMinTrackSize.y = 480;
      }
      return 0;
    case WM_LBUTTONDOWN:
      nativeTelepromptSettingsUiHandleMouseDown(
        hwnd, nativeTelepromptSettingsUiPoint(lParam));
      return 0;
    case WM_MOUSEMOVE:
      nativeTelepromptSettingsUiHandleMouseMove(
        hwnd, nativeTelepromptSettingsUiPoint(lParam));
      return 0;
    case WM_LBUTTONUP:
      nativeTelepromptSettingsUiHandleMouseUp(hwnd);
      return 0;
    case WM_MOUSEWHEEL: {
      if (g_nativeTelepromptSettingsUi.colorOpen ||
          !g_nativeTelepromptSettingsUi.dropdownKey.empty()) {
        return 0;
      }
      const short delta =
        static_cast<short>((wParam >> 16) & 0xffff);
      g_nativeTelepromptSettingsUi.scroll = std::max(
        0, std::min(
          g_nativeTelepromptSettingsUi.maxScroll,
          g_nativeTelepromptSettingsUi.scroll +
            (delta > 0 ? -72 : 72)));
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE) {
        if (g_nativeTelepromptSettingsUi.colorOpen) {
          nativeTelepromptSettingsUiCloseColor();
        } else if (!g_nativeTelepromptSettingsUi.dropdownKey.empty()) {
          g_nativeTelepromptSettingsUi.dropdownKey.clear();
        } else {
          DestroyWindow(hwnd);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == VK_RETURN &&
          g_nativeTelepromptSettingsUi.colorOpen) {
        nativeTelepromptSettingsUiApplyColor();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == VK_UP || wParam == VK_DOWN ||
          wParam == VK_PRIOR || wParam == VK_NEXT) {
        int amount = 56;
        if (wParam == VK_PRIOR || wParam == VK_NEXT) {
          amount = std::max(120,
            static_cast<int>(
              g_nativeTelepromptSettingsUi.viewport.bottom -
              g_nativeTelepromptSettingsUi.viewport.top) - 50);
        }
        const bool upward =
          wParam == VK_UP || wParam == VK_PRIOR;
        g_nativeTelepromptSettingsUi.scroll = std::max(
          0, std::min(
            g_nativeTelepromptSettingsUi.maxScroll,
            g_nativeTelepromptSettingsUi.scroll +
              (upward ? -amount : amount)));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      break;
    case WM_CHAR:
      if (g_nativeTelepromptSettingsUi.colorOpen &&
          g_nativeTelepromptSettingsUi.colorHexFocused) {
        if (wParam == 8) {
          if (g_nativeTelepromptSettingsUi.colorHexReplaceOnType) {
            g_nativeTelepromptSettingsUi.colorHexInput.clear();
            g_nativeTelepromptSettingsUi.colorHexReplaceOnType = false;
          } else if (!g_nativeTelepromptSettingsUi.colorHexInput.empty()) {
            g_nativeTelepromptSettingsUi.colorHexInput.pop_back();
          }
        } else if (std::isxdigit(
              static_cast<unsigned char>(wParam & 0xff))) {
          if (g_nativeTelepromptSettingsUi.colorHexReplaceOnType) {
            g_nativeTelepromptSettingsUi.colorHexInput.clear();
            g_nativeTelepromptSettingsUi.colorHexReplaceOnType = false;
          }
          if (g_nativeTelepromptSettingsUi.colorHexInput.size() < 6) {
            g_nativeTelepromptSettingsUi.colorHexInput +=
              static_cast<char>(std::toupper(
                static_cast<unsigned char>(wParam & 0xff)));
          }
        }
        int red = 0;
        int green = 0;
        int blue = 0;
        if (nativeTelepromptSettingsUiParseHex(
              g_nativeTelepromptSettingsUi.colorHexInput,
              red, green, blue)) {
          g_nativeTelepromptSettingsUi.colorR = red;
          g_nativeTelepromptSettingsUi.colorG = green;
          g_nativeTelepromptSettingsUi.colorB = blue;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (g_nativeTelepromptSettingsUi.page == 2 &&
          g_nativeTelepromptSettingsUi.noticeEmojiFocused) {
        if (wParam == 8) {
          if (!g_nativeTelepromptSettingsUi.notice.emoji.empty()) {
            std::string& emoji =
              g_nativeTelepromptSettingsUi.notice.emoji;
            const size_t previous =
              nativeUiUtf8PreviousBoundary(
                emoji, emoji.size());
            emoji.erase(previous);
          }
        } else if (wParam >= 0x20 &&
                   wParam != 0x7f &&
                   g_nativeTelepromptSettingsUi.notice.emoji.size() < 16) {
          nativeAppendUtf8Codepoint(
            g_nativeTelepromptSettingsUi.notice.emoji,
            static_cast<uint32_t>(wParam));
        }
        nativeTelepromptSettingsUiCommitNotice();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      break;
    case WM_COMMAND:
      if (LOWORD(wParam) == IDCANCEL) {
        DestroyWindow(hwnd);
        return 0;
      }
      break;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
    case WM_NCDESTROY:
      if (message == WM_DESTROY) {
        nativeTelepromptSettingsUiSaveWindowRect(hwnd);
      }
      if (g_nativeTelepromptSettingsUi.hwnd == hwnd) {
        g_nativeTelepromptSettingsUi.hwnd = nullptr;
      }
      nativeTelepromptSettingsUiReleaseBackBuffer();
      g_nativeTelepromptSettingsUi.hits.clear();
      g_nativeTelepromptSettingsUi.dropdownKey.clear();
      nativeTelepromptSettingsUiCloseColor();
      return 0;
  }
#ifdef _WIN32
  return DefWindowProcW(hwnd, message, wParam, lParam);
#else
  return 0;
#endif
}

static void nativeCloseTelepromptSettingsWindow()
{
  HWND hwnd = g_nativeTelepromptSettingsUi.hwnd;
  if (!hwnd || !IsWindow(hwnd)) {
    g_nativeTelepromptSettingsUi.hwnd = nullptr;
    return;
  }
  nativeTelepromptSettingsUiSaveWindowRect(hwnd);
  g_nativeTelepromptSettingsUi.hwnd = nullptr;
  DestroyWindow(hwnd);
  nativeTelepromptSettingsUiReleaseBackBuffer();
}

static bool nativeOpenTelepromptSettingsWindow()
{
  if (nativeTelepromptSettingsUiIsOpen()) {
    ShowWindow(g_nativeTelepromptSettingsUi.hwnd, SW_SHOW);
    SetFocus(g_nativeTelepromptSettingsUi.hwnd);
    return true;
  }

  nativeTelepromptSettingsUiLoadDrafts();
  g_nativeTelepromptSettingsUi.page = 0;
  g_nativeTelepromptSettingsUi.scroll = 0;
  g_nativeTelepromptSettingsUi.maxScroll = 0;
  g_nativeTelepromptSettingsUi.dropdownKey.clear();
  g_nativeTelepromptSettingsUi.noticeEmojiFocused = false;
  g_nativeTelepromptSettingsUi.status =
    "Configuracoes carregadas. Alteracoes sao salvas automaticamente.";
  nativeTelepromptSettingsUiCloseColor();

  const int savedWidth = std::max(
    600, nativeAppActiveReadWindowInt(
      "TP_SETTINGS_WINDOW_W_V1", 920));
  const int savedHeight = std::max(
    480, nativeAppActiveReadWindowInt(
      "TP_SETTINGS_WINDOW_H_V1", 700));
  HWND owner = GetMainHwnd_ptr
    ? GetMainHwnd_ptr() : nullptr;
  int x = nativeAppActiveReadWindowInt(
    "TP_SETTINGS_WINDOW_X_V1", -1);
  int y = nativeAppActiveReadWindowInt(
    "TP_SETTINGS_WINDOW_Y_V1", -1);
  if (x < 0 || y < 0) {
    RECT ownerRect{100, 100,
      100 + savedWidth, 100 + savedHeight};
    if (owner) GetWindowRect(owner, &ownerRect);
    x = ownerRect.left +
      std::max(0, static_cast<int>(
        ownerRect.right - ownerRect.left -
        savedWidth) / 2);
    y = ownerRect.top +
      std::max(0, static_cast<int>(
        ownerRect.bottom - ownerRect.top -
        savedHeight) / 2);
  }

  HWND hwnd = nullptr;
#ifdef _WIN32
  static const wchar_t* className =
    L"VS_HOOK_NATIVE_TELEPROMPT_SETTINGS";
  WNDCLASSEXW windowClass{};
  windowClass.cbSize = sizeof(windowClass);
  if (!GetClassInfoExW(
        g_pluginInstance, className, &windowClass)) {
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc =
      nativeTelepromptSettingsUiWndProc;
    windowClass.hInstance = g_pluginInstance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = className;
    if (!RegisterClassExW(&windowClass)) return false;
  }
  RECT outer{0, 0, savedWidth, savedHeight};
  AdjustWindowRectEx(
    &outer, WS_OVERLAPPEDWINDOW, FALSE, 0);
  hwnd = CreateWindowExW(
    WS_EX_TOOLWINDOW,
    className,
    L"VS Hook - Configuracoes do Teleprompt",
    WS_OVERLAPPEDWINDOW,
    x, y,
    outer.right - outer.left,
    outer.bottom - outer.top,
    owner, nullptr, g_pluginInstance, nullptr);
#else
  const char* resizableWindow =
    reinterpret_cast<const char*>(
      static_cast<INT_PTR>(0x400001));
  hwnd = CreateDialogParam(
    g_pluginInstance, resizableWindow, owner,
    reinterpret_cast<DLGPROC>(
      nativeTelepromptSettingsUiWndProc), 0);
  if (hwnd) {
    SetWindowText(
      hwnd,
      "VS Hook - Configuracoes do Teleprompt");
    SetWindowPos(hwnd, nullptr,
      x, y, savedWidth, savedHeight,
      SWP_NOZORDER | SWP_NOACTIVATE);
  }
#endif
  if (!hwnd) return false;
  g_nativeTelepromptSettingsUi.hwnd = hwnd;
  ShowWindow(hwnd, SW_SHOW);
  SetFocus(hwnd);
  InvalidateRect(hwnd, nullptr, FALSE);
  return true;
}
