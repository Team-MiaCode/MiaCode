# Runs in ScintillaQuick's project scope. Metatype custom commands must belong
# to the dependency's directory so CMake attaches them to its build graph.
cmake_language(DEFER CALL qt_extract_metatypes ScintillaQuick)

function(miacode_scintillaquick_local_coordinates)
    # Scintilla paints in item-local coordinates. Window::GetPosition remains
    # parent-relative for popup placement; GetClientPosition must start at zero
    # when the QML editor moves below the find bar or inside another layout.
    set(platform_source "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/scintillaquick_platqt.cpp")
    file(READ "${platform_source}" platform_code)
    set(original "PRectangle Window::GetClientPosition() const\n{\n    // The client position is the window position\n    return GetPosition();\n}")
    set(replacement "PRectangle Window::GetClientPosition() const\n{\n    QQuickItem* item = resolve_window_item_for_owner(*this);\n    return item ? PRectangle(0, 0, item->width(), item->height())\n                : PRectangle(0, 0, 1000, 1000);\n}")
    string(FIND "${platform_code}" "${original}" match)
    if(match EQUAL -1)
        message(FATAL_ERROR "Update the MiaCode ScintillaQuick client-coordinate patch for this dependency revision")
    endif()
    string(REPLACE "${original}" "${replacement}" patched_code "${platform_code}")
    set(patched_source "${CMAKE_CURRENT_BINARY_DIR}/miacode_scintillaquick_platqt.cpp")
    file(CONFIGURE OUTPUT "${patched_source}" CONTENT "${patched_code}" @ONLY)
    get_target_property(sources ScintillaQuick SOURCES)
    list(REMOVE_ITEM sources src/platform/scintillaquick_platqt.cpp)
    set_property(TARGET ScintillaQuick PROPERTY SOURCES "${sources}")
    target_sources(ScintillaQuick PRIVATE "${patched_source}")
endfunction()
cmake_language(DEFER CALL miacode_scintillaquick_local_coordinates)

function(miacode_scintillaquick_surface_background)
    # Scintilla's style messages only carry RGB. Preserve the public item's
    # optional scene background colour in the captured Scene Graph primitives;
    # QML can then own a single translucent surface beneath text and gutters.
    set(item_source "${CMAKE_CURRENT_SOURCE_DIR}/src/public/scintillaquick_item.cpp")
    file(READ "${item_source}" item_code)
    set(original "    m_render_data->captured_caret_primitives = frame.caret_primitives;")
    set(replacement [=[    const QVariant scene_background = property("sceneBackgroundColor");
    if (scene_background.isValid()) {
        const QColor style_background = QColorFromColourRGBA(m_core->vs.styles[StyleDefault].back);
        const QColor surface_background = scene_background.value<QColor>();
        snapshot.background = surface_background;
        for (auto& band : snapshot.gutter_bands) {
            if (band.color == style_background) band.color = surface_background;
            if (band.pattern_color == style_background) band.pattern_color = surface_background;
        }
        for (auto& primitive : frame.background_primitives) {
            if (primitive.color == style_background && !primitive.marker_underline)
                primitive.color = surface_background;
        }
    }

    const auto editor_palette = property("editorColors").toMap();
    const int active_line = send(SCI_LINEFROMPOSITION, send(SCI_GETCURRENTPOS));
    for (auto& margin : frame.margin_text_primitives) {
        if (margin.style_id == StyleLineNumber)
            margin.foreground = editor_palette.value(margin.document_line == active_line
                ? QStringLiteral("text") : QStringLiteral("lineNumber")).value<QColor>();
    }

    m_render_data->captured_caret_primitives = frame.caret_primitives;]=])
    string(FIND "${item_code}" "${original}" match)
    if(match EQUAL -1)
        message(FATAL_ERROR "Update the MiaCode ScintillaQuick surface-background patch for this dependency revision")
    endif()
    string(REPLACE "${original}" "${replacement}" patched_code "${item_code}")
    string(REPLACE "    std::forward<Apply_update>(apply_update)(scene_graph_update_request(i_message));" [=[    auto update_request = scene_graph_update_request(i_message);
    switch (i_message) {
        case SCI_SETINDICATORCURRENT:
        case SCI_SETINDICATORVALUE:
        case SCI_STARTSTYLING: update_request = {}; break;
        case SCI_INDICATORFILLRANGE:
        case SCI_INDICATORCLEARRANGE: update_request = {true, false, false, false}; break;
        case SCI_SETSTYLINGEX: update_request = {true, true, false, false}; break;
    }
    std::forward<Apply_update>(apply_update)(update_request);]=] patched_code "${patched_code}")
    string(REPLACE "        frame.indicator_primitives   = std::move(m_render_data->frame.indicator_primitives);" "" patched_code "${patched_code}")
    string(REPLACE "            request_scene_graph_update(true, true, false);\n            // `textChanged()`" [=[            const bool indicator_change = int(scn.modificationType) & SC_MOD_CHANGEINDICATOR;
            const bool text_change = int(scn.modificationType) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);
            request_scene_graph_update(!indicator_change, text_change, false);
            // `textChanged()`]=] patched_code "${patched_code}")
    # IME queries and Selection attributes share the current logical-line context.
    foreach(position IN ITEMS pos cur_pos)
        string(REPLACE "m_core->pdoc->ParaUp(${position})"
            "m_core->pdoc->LineStart(m_core->pdoc->SciLineFromPosition(${position}))" patched_code "${patched_code}")
        string(REPLACE "m_core->pdoc->ParaDown(${position})"
            "m_core->pdoc->LineEnd(m_core->pdoc->SciLineFromPosition(${position}))" patched_code "${patched_code}")
    endforeach()
    string(REPLACE "m_core->pdoc->CountUTF16(0, pos)"
        "(m_core->pdoc->IndexLineStart(line, Scintilla::LineCharacterIndexType::Utf16) + m_core->pdoc->CountUTF16(m_core->pdoc->LineStart(line), pos))"
        patched_code "${patched_code}")
    # Layout is resolved before MiaCode restores its viewport and applies navigation.
    set(layout_original [=[void ScintillaQuick_item::updatePolish()
{
    if (m_properties_sync_pending) {
        syncQuickViewProperties();
    }]=])
    set(layout_replacement [=[void ScintillaQuick_item::prepareLayout()
{
    m_core->process_idle_work();
    if (m_properties_sync_pending) syncQuickViewProperties();
    m_core->prepare_layout();
    if (m_properties_sync_pending) syncQuickViewProperties();
}

void ScintillaQuick_item::updatePolish()
{
    prepareLayout();
    captureFrame();
}

void ScintillaQuick_item::captureFrame()
{]=])
    string(REPLACE "${layout_original}" "${layout_replacement}" patched_code "${patched_code}")
    string(REPLACE "    m_core->process_idle_work();\n\n    if (!m_render_data->static_content_dirty"
        "    if (!m_render_data->static_content_dirty" patched_code "${patched_code}")
    set(generated_include "${CMAKE_CURRENT_BINARY_DIR}/miacode_include")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/scintillaquick/scintillaquick_item.h" item_header)
    string(REPLACE "    void updatePolish() override;" "    void prepareLayout();\n    void captureFrame();\n    void updatePolish() override;" item_header "${item_header}")
    set(display_layout_signal "    void verticalRangeChanged(int max, int page);")
    string(REPLACE "${display_layout_signal}" "${display_layout_signal}\n    // Emitted after display layout synchronizes scroll ranges, including unchanged ranges.\n    void displayLayoutChanged();" item_header "${item_header}")
    set(display_layout_connection "    connect(m_core, SIGNAL(notifyChange()), this, SIGNAL(notifyChange()));")
    string(REPLACE "${display_layout_connection}" "    connect(m_core, &ScintillaQuick_core::displayLayoutChanged, this, &ScintillaQuick_item::displayLayoutChanged);\n\n${display_layout_connection}" patched_code "${patched_code}")
    file(CONFIGURE OUTPUT "${generated_include}/scintillaquick/scintillaquick_item.h" CONTENT "${item_header}" @ONLY)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/core/scintillaquick_core.h" core_header)
    string(REPLACE "public:\n" "public:\n    void prepare_layout() { RefreshStyleData(); WrapLines(WrapScope::wsVisible); }\n" core_header "${core_header}")
    string(REPLACE "${display_layout_signal}" "${display_layout_signal}\n    void displayLayoutChanged();" core_header "${core_header}")
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scintillaquick_core.h" CONTENT "${core_header}" @ONLY)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/core/scintillaquick_core.cpp" core_code)
    set(display_layout_original [=[    return modified;
}

void ScintillaQuick_core::CopyToModeClipboard]=])
    set(display_layout_replacement [=[    // Display-row heights may change while the total scroll range stays equal.
    emit displayLayoutChanged();
    return modified;
}

void ScintillaQuick_core::CopyToModeClipboard]=])
    string(FIND "${core_code}" "${display_layout_original}" match)
    if(match EQUAL -1)
        message(FATAL_ERROR "Update the MiaCode ScintillaQuick display-layout patch for this dependency revision")
    endif()
    string(REPLACE "${display_layout_original}" "${display_layout_replacement}" core_code "${core_code}")
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/miacode_scintillaquick_core.cpp" CONTENT "${core_code}" @ONLY)
    target_include_directories(ScintillaQuick BEFORE PUBLIC "$<BUILD_INTERFACE:${generated_include}>")
    set(patched_source "${CMAKE_CURRENT_BINARY_DIR}/miacode_scintillaquick_item.cpp")
    file(CONFIGURE OUTPUT "${patched_source}" CONTENT "${patched_code}" @ONLY)
    get_target_property(sources ScintillaQuick SOURCES)
    list(REMOVE_ITEM sources src/public/scintillaquick_item.cpp src/core/scintillaquick_core.cpp
        src/core/scintillaquick_core.h include/scintillaquick/scintillaquick_item.h)
    set_property(TARGET ScintillaQuick PROPERTY SOURCES "${sources}")
    target_sources(ScintillaQuick PRIVATE "${patched_source}"
        "${CMAKE_CURRENT_BINARY_DIR}/miacode_scintillaquick_core.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/scintillaquick_core.h"
        "${generated_include}/scintillaquick/scintillaquick_item.h")
endfunction()
cmake_language(DEFER CALL miacode_scintillaquick_surface_background)

function(miacode_scintillaquick_highlight_layers)
    set(renderer_source "${CMAKE_CURRENT_SOURCE_DIR}/src/render/scintillaquick_scene_graph_renderer.cpp")
    file(READ "${renderer_source}" renderer_code)
    set(original "        appendChildNode(m_text_clip_node);")
    set(replacement [=[        m_current_line_clip_node = new QSGClipNode();
        appendChildNode(m_current_line_clip_node);
        m_current_line_clip_node->appendChildNode(m_current_line_groups[1]);
        appendChildNode(m_text_clip_node);]=])
    string(FIND "${renderer_code}" "${original}" match)
    if(match EQUAL -1)
        message(FATAL_ERROR "Update the MiaCode ScintillaQuick highlight-layer patch for this dependency revision")
    endif()
    string(REPLACE "${original}" "${replacement}" renderer_code "${renderer_code}")
    string(REPLACE "        m_text_clip_node->appendChildNode(m_current_line_groups[1]);" "" renderer_code "${renderer_code}")
    string(REPLACE "        update_clip_node(m_text_clip_node, frame.text_rect);" [=[        update_clip_node(m_text_clip_node, frame.text_rect);
        update_clip_node(m_current_line_clip_node,
            QRectF(frame.text_rect.left(), frame.text_rect.top(),
                snapshot.item_size.width() - frame.text_rect.left(), frame.text_rect.height()));]=] renderer_code "${renderer_code}")
    string(REPLACE "                    current_lines.push_back({primitive.rect, primitive.color});" [=[                    QRectF rect = primitive.rect;
                    if (primitive.layer == Layer::UnderText) rect.setRight(snapshot.item_size.width());
                    current_lines.push_back({rect, primitive.color});]=] renderer_code "${renderer_code}")
    string(REPLACE "        for (const Indicator_primitive& indicator : frame.indicator_primitives) {" [=[        for (const Indicator_primitive& indicator : frame.indicator_primitives) {
            if (indicator.indicator_number == 11) continue;]=] renderer_code "${renderer_code}")
    string(REPLACE "    QSGNode* m_indicator_under_group = nullptr;" "    QSGClipNode* m_current_line_clip_node = nullptr;\n    QSGNode* m_indicator_under_group = nullptr;" renderer_code "${renderer_code}")
    # Indicator geometry updates independently from cached glyph nodes.
    string(FIND "${renderer_code}" "        std::vector<const Indicator_primitive*> under_indicators;" indicator_begin)
    string(FIND "${renderer_code}" "        sync_frame_text_nodes(window, m_marker_group" indicator_end)
    math(EXPR indicator_length "${indicator_end} - ${indicator_begin}")
    string(SUBSTRING "${renderer_code}" ${indicator_begin} ${indicator_length} indicator_code)
    string(REPLACE "${indicator_code}" "" renderer_code "${renderer_code}")
    string(REPLACE "        const qreal static_dpr = window->effectiveDevicePixelRatio();"
        "${indicator_code}        const qreal static_dpr = window->effectiveDevicePixelRatio();" renderer_code "${renderer_code}")
    # Refresh captured line-number colours during caret and selection updates.
    string(FIND "${renderer_code}" "            // Gutter text from frame margin text primitives" gutter_begin)
    string(FIND "${renderer_code}" "                    node->update_from_margin_text(window, margin, frame.margin_rect);" gutter_update)
    string(SUBSTRING "${renderer_code}" ${gutter_update} -1 gutter_tail)
    string(FIND "${gutter_tail}" "                });" gutter_close)
    math(EXPR gutter_length "${gutter_update} - ${gutter_begin} + ${gutter_close} + 19")
    string(SUBSTRING "${renderer_code}" ${gutter_begin} ${gutter_length} gutter_code)
    string(REPLACE "${gutter_code}" "" renderer_code "${renderer_code}")
    string(REPLACE "        const qreal static_dpr = window->effectiveDevicePixelRatio();"
        "${gutter_code}\n        const qreal static_dpr = window->effectiveDevicePixelRatio();" renderer_code "${renderer_code}")
    # Dedicated captured ranges update fixed contour nodes beneath glyphs.
    string(PREPEND renderer_code "#include \"app/ui/editor/ScintillaSelectionRenderer.h\"\n")
    string(REPLACE "        for (size_t layer = 0; layer < m_selection_groups.size(); ++layer) {"
        "        for (size_t layer = 0; layer < m_selection_groups.size(); ++layer) {\n            if (layer == 1) continue;" renderer_code "${renderer_code}")
    set(highlight_code [=[        QVector<QRectF> selection_rectangles, follow_rectangles;
        QColor selection_color, follow_color;
        for (const auto& selection : frame.selection_primitives) {
            if (selection.layer == Layer::UnderText && !selection.rect.isEmpty()) {
                selection_rectangles.append(selection.rect);
                selection_color = selection.color;
            }
        }
        for (const auto& indicator : frame.indicator_primitives) {
            if (indicator.indicator_number == 11 && !indicator.line_rect.isEmpty()) {
                follow_rectangles.append(indicator.line_rect);
                follow_color = indicator.color;
                follow_color.setAlpha(indicator.fill_alpha);
            }
        }
        miacode::ui::synchronizeScintillaHighlight(m_highlight_nodes[0], m_selection_groups[1],
            window, selection_rectangles, selection_color);
        miacode::ui::synchronizeScintillaHighlight(m_highlight_nodes[1], m_selection_groups[1],
            window, follow_rectangles, follow_color);

]=])
    string(REPLACE "        // Caret rectangles from frame" "${highlight_code}        // Caret rectangles from frame" renderer_code "${renderer_code}")
    string(REPLACE "    std::array<QSGNode*, 3> m_selection_groups{};"
        "    std::array<QSGNode*, 3> m_selection_groups{};\n    std::array<QSGNode*, 2> m_highlight_nodes{};" renderer_code "${renderer_code}")
    target_sources(ScintillaQuick PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/app/ui/editor/ScintillaSelectionRenderer.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/app/ui/editor/ScintillaSelectionRenderer.h")
    target_include_directories(ScintillaQuick PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src")
    set(patched_source "${CMAKE_CURRENT_BINARY_DIR}/miacode_scintillaquick_scene_graph_renderer.cpp")
    file(CONFIGURE OUTPUT "${patched_source}" CONTENT "${renderer_code}" @ONLY)
    get_target_property(sources ScintillaQuick SOURCES)
    list(REMOVE_ITEM sources src/render/scintillaquick_scene_graph_renderer.cpp)
    set_property(TARGET ScintillaQuick PROPERTY SOURCES "${sources}")
    target_sources(ScintillaQuick PRIVATE "${patched_source}")
endfunction()
cmake_language(DEFER CALL miacode_scintillaquick_highlight_layers)

function(miacode_scintillaquick_indicator_capture)
    set(view_source "${CMAKE_CURRENT_SOURCE_DIR}/third_party/scintilla/src/EditView.cxx")
    file(READ "${view_source}" view_code)
    set(original [=[			if (collector->wants_static_content()) {
				collector->add_indicator_primitive(capturedInd);
			}]=])
    string(REPLACE "${original}" "\t\t\tcollector->add_indicator_primitive(capturedInd);" view_code "${view_code}")
    set(patched_source "${CMAKE_CURRENT_BINARY_DIR}/miacode_scintilla_edit_view.cpp")
    file(CONFIGURE OUTPUT "${patched_source}" CONTENT "${view_code}" @ONLY)
    get_target_property(sources scintillaquick_scintilla_objects SOURCES)
    list(REMOVE_ITEM sources third_party/scintilla/src/EditView.cxx)
    set_property(TARGET scintillaquick_scintilla_objects PROPERTY SOURCES "${sources}")
    target_sources(scintillaquick_scintilla_objects PRIVATE "${patched_source}")
endfunction()
cmake_language(DEFER CALL miacode_scintillaquick_indicator_capture)
