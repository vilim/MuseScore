/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "read400.h"

#include "io/xml.h"

#include "libmscore/score.h"
#include "libmscore/masterscore.h"
#include "libmscore/audio.h"
#include "libmscore/text.h"
#include "libmscore/part.h"
#include "libmscore/spanner.h"
#include "libmscore/excerpt.h"
#include "libmscore/staff.h"
#include "libmscore/factory.h"
#include "libmscore/measure.h"

using namespace mu::engraving;
using namespace Ms;

bool Read400::read400(Ms::Score* score, XmlReader& e, ReadContext& ctx)
{
    if (!e.readNextStartElement()) {
        qDebug("%s: xml file is empty", qPrintable(e.getDocName()));
        return false;
    }

    if (e.name() == "museScore") {
        while (e.readNextStartElement()) {
            const QStringRef& tag(e.name());
            if (tag == "programVersion") {
                e.skipCurrentElement();
            } else if (tag == "programRevision") {
                e.skipCurrentElement();
            } else if (tag == "Revision") {
                e.skipCurrentElement();
            } else if (tag == "Score") {
                if (!readScore400(score, e, ctx)) {
                    return false;
                }
            } else {
                e.skipCurrentElement();
            }
        }
    } else {
        qDebug("%s: invalid structure of xml file", qPrintable(e.getDocName()));
        return false;
    }

    return true;
}

bool Read400::readScore400(Ms::Score* score, XmlReader& e, ReadContext& ctx)
{
    // HACK
    // style setting compatibility settings for minor versions
    // this allows new style settings to be added
    // with different default values for older vs newer scores
    // note: older templates get the default values for older scores
    // these can be forced back in MuseScore::getNewFile() if necessary
    QString programVersion = score->masterScore()->mscoreVersion();
    bool disableHarmonyPlay = MScore::harmonyPlayDisableCompatibility && !MScore::testMode;
    if (!programVersion.isEmpty() && programVersion < "3.5" && disableHarmonyPlay) {
        score->style().set(Sid::harmonyPlay, false);
    }

    while (e.readNextStartElement()) {
        e.setTrack(-1);
        const QStringRef& tag(e.name());
        if (tag == "Staff") {
            readStaff(score, e, ctx);
        } else if (tag == "Omr") {
            e.skipCurrentElement();
        } else if (tag == "Audio") {
            score->_audio = new Audio;
            score->_audio->read(e);
        } else if (tag == "showOmr") {
            e.skipCurrentElement();
        } else if (tag == "playMode") {
            score->_playMode = PlayMode(e.readInt());
        } else if (tag == "LayerTag") {
            int id = e.intAttribute("id");
            const QString& t = e.attribute("tag");
            QString val(e.readElementText());
            if (id >= 0 && id < 32) {
                score->_layerTags[id] = t;
                score->_layerTagComments[id] = val;
            }
        } else if (tag == "Layer") {
            Layer layer;
            layer.name = e.attribute("name");
            layer.tags = e.attribute("mask").toUInt();
            score->_layer.append(layer);
            e.readNext();
        } else if (tag == "currentLayer") {
            score->_currentLayer = e.readInt();
        } else if (tag == "Synthesizer") {
            score->_synthesizerState.read(e);
        } else if (tag == "page-offset") {
            score->_pageNumberOffset = e.readInt();
        } else if (tag == "Division") {
            score->_fileDivision = e.readInt();
        } else if (tag == "showInvisible") {
            score->_showInvisible = e.readInt();
        } else if (tag == "showUnprintable") {
            score->_showUnprintable = e.readInt();
        } else if (tag == "showFrames") {
            score->_showFrames = e.readInt();
        } else if (tag == "showMargins") {
            score->_showPageborders = e.readInt();
        } else if (tag == "markIrregularMeasures") {
            score->_markIrregularMeasures = e.readInt();
        } else if (tag == "Style") {
            // Since version 400, the style is stored in a separate file
            e.skipCurrentElement();
        } else if (tag == "copyright" || tag == "rights") {
            score->setMetaTag("copyright", Text::readXmlText(e, score));
        } else if (tag == "movement-number") {
            score->setMetaTag("movementNumber", e.readElementText());
        } else if (tag == "movement-title") {
            score->setMetaTag("movementTitle", e.readElementText());
        } else if (tag == "work-number") {
            score->setMetaTag("workNumber", e.readElementText());
        } else if (tag == "work-title") {
            score->setMetaTag("workTitle", e.readElementText());
        } else if (tag == "source") {
            score->setMetaTag("source", e.readElementText());
        } else if (tag == "metaTag") {
            QString name = e.attribute("name");
            score->setMetaTag(name, e.readElementText());
        } else if (tag == "Order") {
            ScoreOrder order;
            order.read(e);
            if (order.isValid()) {
                score->setScoreOrder(order);
            }
        } else if (tag == "Part") {
            Part* part = new Part(score);
            part->read(e);
            score->appendPart(part);
        } else if ((tag == "HairPin")
                   || (tag == "Ottava")
                   || (tag == "TextLine")
                   || (tag == "Volta")
                   || (tag == "Trill")
                   || (tag == "Slur")
                   || (tag == "Pedal")) {
            Spanner* s = toSpanner(Factory::createItemByName(tag, score->dummy()));
            s->read(e);
            score->addSpanner(s);
        } else if (tag == "Excerpt") {
            // Since version 400, the Excerpts are stored in a separate file
            e.skipCurrentElement();
        } else if (e.name() == "Tracklist") {
            int strack = e.intAttribute("sTrack",   -1);
            int dtrack = e.intAttribute("dstTrack", -1);
            if (strack != -1 && dtrack != -1) {
                e.tracks().insert(strack, dtrack);
            }
            e.skipCurrentElement();
        } else if (tag == "Score") {
            // Since version 400, the Excerpts is stored in a separate file
            e.skipCurrentElement();
        } else if (tag == "name") {
            QString n = e.readElementText();
            if (!score->isMaster()) {     //ignore the name if it's not a child score
                score->excerpt()->setTitle(n);
            }
        } else if (tag == "layoutMode") {
            QString s = e.readElementText();
            if (s == "line") {
                score->setLayoutMode(LayoutMode::LINE);
            } else if (s == "system") {
                score->setLayoutMode(LayoutMode::SYSTEM);
            } else {
                qDebug("layoutMode: %s", qPrintable(s));
            }
        } else {
            e.unknown();
        }
    }
    e.reconnectBrokenConnectors();
    if (e.error() != QXmlStreamReader::NoError) {
        qDebug("%s: xml read error at line %lld col %lld: %s",
               qPrintable(e.getDocName()), e.lineNumber(), e.columnNumber(),
               e.name().toUtf8().data());
        if (e.error() == QXmlStreamReader::CustomError) {
            MScore::lastError = e.errorString();
        } else {
            MScore::lastError = QObject::tr("XML read error at line %1, column %2: %3").arg(e.lineNumber()).arg(e.columnNumber()).arg(
                e.name().toString());
        }
        return false;
    }

    score->connectTies();
    score->relayoutForStyles(); // force relayout if certain style settings are enabled

    score->_fileDivision = MScore::division;

    // Make sure every instrument has an instrumentId set.
    for (Part* part : score->parts()) {
        const InstrumentList* il = part->instruments();
        for (auto it = il->begin(); it != il->end(); it++) {
            static_cast<Instrument*>(it->second)->updateInstrumentId();
        }
    }

    score->fixTicks();

    for (Part* p : qAsConst(score->_parts)) {
        p->updateHarmonyChannels(false);
    }

    score->masterScore()->rebuildMidiMapping();
    score->masterScore()->updateChannel();

    for (Staff* staff : score->staves()) {
        staff->updateOttava();
    }

//      createPlayEvents();

    return true;
}

void Read400::readStaff(Ms::Score* score, Ms::XmlReader& e, ReadContext& ctx)
{
    int staff = e.intAttribute("id", 1) - 1;
    int measureIdx = 0;
    e.setCurrentMeasureIndex(0);
    e.setTick(Fraction(0, 1));
    e.setTrack(staff * VOICES);

    if (staff == 0) {
        while (e.readNextStartElement()) {
            const QStringRef& tag(e.name());

            if (tag == "Measure") {
                Measure* measure = Factory::createMeasure(score->dummy()->system());
                measure->setTick(e.tick());
                e.setCurrentMeasureIndex(measureIdx++);
                //
                // inherit timesig from previous measure
                //
                Measure* m = e.lastMeasure();             // measure->prevMeasure();
                Fraction f(m ? m->timesig() : Fraction(4, 4));
                measure->setTicks(f);
                measure->setTimesig(f);

                measure->read(e, staff);
                measure->checkMeasure(staff);
                if (!measure->isMMRest()) {
                    score->measures()->add(measure);
                    e.setLastMeasure(measure);
                    e.setTick(measure->tick() + measure->ticks());
                } else {
                    // this is a multi measure rest
                    // always preceded by the first measure it replaces
                    Measure* m1 = e.lastMeasure();

                    if (m1) {
                        m1->setMMRest(measure);
                        measure->setTick(m1->tick());
                    }
                }
            } else if (tag == "HBox" || tag == "VBox" || tag == "TBox" || tag == "FBox") {
                MeasureBase* mb = toMeasureBase(Factory::createItemByName(tag, score->dummy()));
                mb->read(e);
                mb->setTick(e.tick());
                score->measures()->add(mb);
            } else if (tag == "tick") {
                e.setTick(Fraction::fromTicks(score->fileDivision(e.readInt())));
            } else {
                e.unknown();
            }
        }
    } else {
        Measure* measure = score->firstMeasure();
        while (e.readNextStartElement()) {
            const QStringRef& tag(e.name());

            if (tag == "Measure") {
                if (measure == 0) {
                    qDebug("Score::readStaff(): missing measure!");
                    measure = Factory::createMeasure(score->dummy()->system());
                    measure->setTick(e.tick());
                    score->measures()->add(measure);
                }
                e.setTick(measure->tick());
                e.setCurrentMeasureIndex(measureIdx++);
                measure->read(e, staff);
                measure->checkMeasure(staff);
                if (measure->isMMRest()) {
                    measure = e.lastMeasure()->nextMeasure();
                } else {
                    e.setLastMeasure(measure);
                    if (measure->mmRest()) {
                        measure = measure->mmRest();
                    } else {
                        measure = measure->nextMeasure();
                    }
                }
            } else if (tag == "tick") {
                e.setTick(Fraction::fromTicks(score->fileDivision(e.readInt())));
            } else {
                e.unknown();
            }
        }
    }
}
