/**
 *    Copyright (C) 2013 10gen Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "mongo/db/clientcursor.h"
#include "mongo/db/query/canonical_query.h"
#include "mongo/db/query/lite_parsed_query.h"
#include "mongo/db/query/plan_cache.h"
#include "mongo/db/query/plan_executor.h"
#include "mongo/db/query/runner.h"
#include "mongo/db/query/stage_builder.h"

namespace mongo {

    /**
     * SingleSolutionRunner runs a plan that was the only possible solution to a query.  It exists
     * only to dump stats into the cache after running.
     */
    class SingleSolutionRunner : public Runner {
    public:
        /**
         * Takes ownership of both arguments.
         */
        SingleSolutionRunner(CanonicalQuery* canonicalQuery, QuerySolution* soln,
                             PlanStage* root, WorkingSet* ws)
            : _canonicalQuery(canonicalQuery), _solution(soln),
              _exec(new PlanExecutor(ws, root)), _killed(false) { }

        Runner::RunnerState getNext(BSONObj* objOut) {
            if (_killed) { return Runner::RUNNER_DEAD; }
            return _exec->getNext(objOut);
            // TODO: I'm not convinced we want to cache this run.  What if it's a collscan solution
            // and the user adds an index later?  We don't want to reach for this.  But if solving
            // the query is v. hard, we do want to cache it.  Maybe we can remove single solution
            // cache entries when we build an index?
        }

        virtual void saveState() {
            if (!_killed) {
                _exec->saveState();
            }
        }

        virtual void restoreState() {
            if (!_killed) {
                _exec->restoreState();
            }
        }

        virtual void invalidate(const DiskLoc& dl) {
            if (!_killed) {
                _exec->invalidate(dl);
            }
        }

        virtual const CanonicalQuery& getQuery() { return *_canonicalQuery; }

        virtual void kill() { _killed = true; }

        virtual bool forceYield() {
            saveState();
            ClientCursor::registerRunner(this);
            ClientCursor::staticYield(ClientCursor::suggestYieldMicros(),
                                      getQuery().getParsed().ns(),
                                      NULL);
            ClientCursor::deregisterRunner(this);
            if (!_killed) { restoreState(); }
            return !_killed;
        }

    private:
        scoped_ptr<CanonicalQuery> _canonicalQuery;
        scoped_ptr<QuerySolution> _solution;
        scoped_ptr<PlanExecutor> _exec;

        // Were we killed during a yield?
        bool _killed;
    };

}  // namespace mongo

