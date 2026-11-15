#pragma once

// Which font each Markdown face is on the device. Separate from NotesScreens so
// the screens stay free of the renderer.

#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "NotesScreens.h"

namespace notesui {

inline void bindNoteFace(const fui::DrawTarget& target, const NoteFace face) {
  int font = toybox::kReadingFontId;
  switch (face) {
    case NoteFace::Body:
      break;
    case NoteFace::Bold:
      font = toybox::kNotesBoldFontId;
      break;
    case NoteFace::Italic:
      font = toybox::kNotesItalicFontId;
      break;
    case NoteFace::Code:
      font = toybox::kNotesCodeFontId;
      break;
    case NoteFace::Math:
      font = toybox::kNotesMathFontId;
      break;
    case NoteFace::MathSmall:
      font = toybox::kNotesMathSmallFontId;
      break;
    case NoteFace::Heading:
      font = toybox::kReadingBoldFontId;
      break;
  }
  // The target is only ever the GfxRendererTarget the Activity made, and the
  // slot is a binding rather than drawn state, which is why measuring may set it.
  const_cast<fui::GfxRendererTarget&>(static_cast<const fui::GfxRendererTarget&>(target)).setFont(3, font);
}

}  // namespace notesui
