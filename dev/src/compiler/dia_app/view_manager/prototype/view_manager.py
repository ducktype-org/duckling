import threading
import grpc
from concurrent import futures
import view_pb2
import view_pb2_grpc

view_lock = threading.Lock()
interactive_states = {}
side_notes = []

TOGGLE_ID = 100
NESTED_INTERACTIVE_ID = 300
SIDE_ENTRY_ID_HL = 200
SIDE_ENTRY_ID_FULL = 201

def build_view_response():
    diags = []

    sections = []
    def sep(title):
        dash = view_pb2.Component(
            text_component=view_pb2.TextComponent(content='--------', hl_tags=[])
        )
        title_c = view_pb2.Component(
            text_component=view_pb2.TextComponent(content=title, hl_tags=[])
        )
        sections.append(view_pb2.Section(text_section=view_pb2.TextSection(root=dash)))
        sections.append(view_pb2.Section(text_section=view_pb2.TextSection(root=title_c)))

    metadata = view_pb2.Metadata(error_code='E001', file_info='file.py:10')

    sep('Test: Overlapping highlights')
    a = view_pb2.Component(text_component=view_pb2.TextComponent(content='A', hl_tags=[1]))
    b = view_pb2.Component(text_component=view_pb2.TextComponent(content='B', hl_tags=[1,2]))
    ov = view_pb2.Component(concat_component=view_pb2.ConcatComponent(components=[a,b], hl_tags=[]))
    sections.append(view_pb2.Section(text_section=view_pb2.TextSection(root=ov)))

    sep('Test: No-HL text')
    nh = view_pb2.NoHlComponent(text_component=view_pb2.NoHlTextComponent(content='Plain no-HL text'))
    sections.append(view_pb2.Section(no_hl_text_section=view_pb2.NoHlTextSection(root=nh)))

    sep('Test: Code section')
    lines = []
    for i in (1,2,3):
        comp = view_pb2.Component(
            code_component=view_pb2.CodeComponent(content=f'    code{i}', hl_tags=[3] if i==1 else [])
        )
        lines.append(view_pb2.CodeLine(line_number=i, content=view_pb2.TextSection(root=comp)))
    sections.append(view_pb2.Section(code_section=view_pb2.CodeSection(lines=lines)))

    sep('Test: Concat component')
    c1 = view_pb2.Component(text_component=view_pb2.TextComponent(content='X', hl_tags=[]))
    c2 = view_pb2.Component(code_component=view_pb2.CodeComponent(content='Y', hl_tags=[]))
    cc = view_pb2.Component(concat_component=view_pb2.ConcatComponent(components=[c1,c2], hl_tags=[]))
    sections.append(view_pb2.Section(text_section=view_pb2.TextSection(root=cc)))

    sep('Test: Interactive nested')
    outer_on = interactive_states.get(TOGGLE_ID, False)
    outer_label = 'Outer B' if outer_on else 'Outer A'
    outer_comp = view_pb2.Component(text_component=view_pb2.TextComponent(content=outer_label, hl_tags=[]))
    outer = view_pb2.Component(
        interactive_component=view_pb2.InteractiveComponent(
            component_id=TOGGLE_ID,
            primary_component=outer_comp
        )
    )
    nested_on = interactive_states.get(NESTED_INTERACTIVE_ID, False)
    inner_label = 'Inner B' if nested_on else 'Inner A'
    inner_comp = view_pb2.Component(text_component=view_pb2.TextComponent(content=inner_label, hl_tags=[]))
    inner = view_pb2.Component(
        interactive_component=view_pb2.InteractiveComponent(
            component_id=NESTED_INTERACTIVE_ID,
            primary_component=inner_comp
        )
    )
    combo = view_pb2.Component(concat_component=view_pb2.ConcatComponent(components=[outer, inner], hl_tags=[]))
    sections.append(view_pb2.Section(text_section=view_pb2.TextSection(root=combo)))

    sep('Test: Group span across sections')
    for text, tags in [('Span1',[10]), ('Middle text',None), ('Span2',[10, 33])]:
        comp = view_pb2.Component(text_component=view_pb2.TextComponent(content=text, hl_tags=tags))
        sections.append(view_pb2.Section(text_section=view_pb2.TextSection(root=comp)))

    sep('Test: Simple note')
    se1 = view_pb2.Component(side_entry_component=view_pb2.SideEntryComponent(side_entry_id=SIDE_ENTRY_ID_HL))
    sections.append(view_pb2.Section(text_section=view_pb2.TextSection(root=se1)))

    sep('Test: Full note')
    se2 = view_pb2.Component(side_entry_component=view_pb2.SideEntryComponent(side_entry_id=SIDE_ENTRY_ID_FULL))
    sections.append(view_pb2.Section(text_section=view_pb2.TextSection(root=se2)))


    sep('Test tag inheritance')
    top_con = view_pb2.ConcatComponent(components=[
        view_pb2.Component(text_component=view_pb2.TextComponent(content="Text1", hl_tags = [1337, 1313])),
        view_pb2.Component(text_component=view_pb2.TextComponent(content="  - text2 -  ", hl_tags = [10])),
        view_pb2.Component(text_component=view_pb2.TextComponent(content="Text3", hl_tags = [1337])),
    ],
    hl_tags=[1313])

    sections.append(view_pb2.Section(text_section=view_pb2.TextSection(root=view_pb2.Component(concat_component=top_con))))

    msg10 = view_pb2.HlInfo(tag=10, message="Sample hover message")
    msg1337 = view_pb2.HlInfo(tag=1337, message="MSG 1337")
    msg1313 = view_pb2.HlInfo(tag=1313, message="MSG 1313")

    msgs = [msg10, msg1337, msg1313]

    diags.append(view_pb2.Diagnostic(metadata=metadata, sections=sections, hl_messages=msgs))
    return view_pb2.ViewResponse(diagnostics=diags, side_notes=list(side_notes))

class ViewServiceServicer(view_pb2_grpc.ViewServiceServicer):
    def GetView(self, request, context):
        return build_view_response()

    def Click(self, request, context):
        cid = request.component_id
        with view_lock:
            if cid in (TOGGLE_ID, NESTED_INTERACTIVE_ID):
                interactive_states[cid] = not interactive_states.get(cid, False)
            elif cid == SIDE_ENTRY_ID_HL:
                sn = view_pb2.SideNoHlComponent(
                    text_component=view_pb2.NoHlTextComponent(content='Simple note text')
                )
                note = view_pb2.SideNote(
                    diagnostics=[view_pb2.SideDiagnostic(
                        metadata=view_pb2.SideMetadata(note_code='SN1'),
                        sections=[view_pb2.SideSection(no_hl_text_section=view_pb2.SideNoHlTextSection(root=sn))],
                        edges=[]
                    )]
                )
                side_notes.append(note)
            elif cid == SIDE_ENTRY_ID_FULL:
                s1 = view_pb2.SideSection(
                    no_hl_text_section=view_pb2.SideNoHlTextSection(
                        root=view_pb2.SideNoHlComponent(
                            text_component=view_pb2.NoHlTextComponent(content='SN NoHL')
                        )
                    )
                )
                s2 = view_pb2.SideSection(
                    text_section=view_pb2.SideTextSection(
                        root=view_pb2.SideComponent(
                            text_component=view_pb2.TextComponent(content='SN Text', hl_tags=[5])
                        )
                    )
                )
                cl = view_pb2.SideCodeLine(
                    line_number=9,
                    content=view_pb2.SideTextSection(
                        root=view_pb2.SideComponent(
                            code_component=view_pb2.CodeComponent(content='  SNCode()', hl_tags=[10])
                        )
                    )
                )
                s3 = view_pb2.SideSection(code_section=view_pb2.SideCodeSection(lines=[cl]))
                sca = view_pb2.SideComponent(text_component=view_pb2.TextComponent(content='CA', hl_tags=[]))
                scb = view_pb2.SideComponent(code_component=view_pb2.CodeComponent(content='CB', hl_tags=[]))
                concat = view_pb2.SideConcatComponent(components=[sca, scb], hl_tags=[])
                s4 = view_pb2.SideSection(text_section=view_pb2.SideTextSection(root=view_pb2.SideComponent(concat_component=concat)))
                si = view_pb2.SideInteractiveComponent(
                    component_id=500,
                    primary_component=view_pb2.SideComponent(
                        text_component=view_pb2.TextComponent(content='SI on', hl_tags=[])
                    )
                )
                s5 = view_pb2.SideSection(text_section=view_pb2.SideTextSection(root=view_pb2.SideComponent(interactive_component=si)))
                parent = view_pb2.SideDiagnostic(
                    metadata=view_pb2.SideMetadata(note_code='P SN'),
                    sections=[s1, s2, s3, s4, s5],
                    edges=[view_pb2.SideEdge(side_note_id=len(side_notes)+1, description='Child')]
                )
                child = view_pb2.SideDiagnostic(
                    metadata=view_pb2.SideMetadata(note_code='C SN'),
                    sections=[s1],
                    edges=[]
                )
                side_notes.append(view_pb2.SideNote(diagnostics=[parent, child]))
        return view_pb2.ClickResponse(status='OK')

    def CloseSideNote(self, request, context):
        sid = request.side_note_id
        with view_lock:
            if 0 <= sid < len(side_notes):
                side_notes[:] = side_notes[:sid]
        return view_pb2.CloseSideNoteResponse(status='Closed')

if __name__ == '__main__':
    serve = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    view_pb2_grpc.add_ViewServiceServicer_to_server(ViewServiceServicer(), serve)
    serve.add_insecure_port('[::]:50051')
    serve.start()
    serve.wait_for_termination()
