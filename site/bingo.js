function assert(value) {
	if (!value) {
		debugger;
		alert("Assertion failed");
	}
}

function shorthandLookup(strShort) {
	for (let i = 0; i < manifest.shorthands.length; i++) {
		if (manifest.shorthands[i].short === strShort) {
			return manifest.shorthands[i].long;
		}
	}

	return "";
}

function htmlFromStringWithShorthands(str) {
	let iLBrack = -1;
	let iRBrack = -1;
	let result = "";

	for (let i = 0; i < str.length; i++) {
		let c = str[i];

		if (c === "[") {
			iLBrack = i;
		}
		else if (c === "]") {
			iRBrack = i;

			if (iLBrack !== -1) {
				let strShort = str.substring(iLBrack + 1, iRBrack);
				result += "<span class='ttCrumb'>" + strShort + "<span class='ttText'>" + shorthandLookup(strShort) + "</span></span>";
				iLBrack = -1;
				iRBrack = -1;
			}
		}
		else if (iLBrack === -1) {
			result += c;
		}
	}

	return result;
}

function htmlFromGoal(goal) {
	let result = htmlFromStringWithShorthands(goal.text);

	if (goal.alt !== "") {
		result += " <span class='ttCrumb'>[?]<span class='ttText'>" + htmlFromStringWithShorthands(goal.alt) + "</span></span>";
	}

	return result;
}

function initLayout() {

	// Set cell size

	{
		let dimension = 0.7 * Math.min(window.innerWidth, window.innerHeight);
		dimension = Math.max(400, dimension);
		dimension = Math.min(800, dimension);

		let cellDimension = dimension / 5.0;

		let cells = document.getElementsByTagName("td");
		for (let i = 0; i < cells.length; i++) {
			cells[i].style.width = cellDimension + "px";
			cells[i].style.height = cellDimension + "px";
		}
	}

	// Resize right panel

	let panelCard = document.getElementById("panelCard");
	let panelRight = document.getElementById("panelRight");

	panelRight.style.height = panelCard.offsetHeight + 0.5 * (window.innerHeight - panelCard.offsetHeight) + "px";
	panelRight.style.width = (window.innerWidth - panelCard.offsetWidth - 50) + "px";
}

function initHandlers() {

	// Get random seed from URL or generate one

	if (window.location.href.includes("?seed=")) {
		let seedIndex = window.location.href.indexOf("?seed=");
		seedIndex += "?seed=".length;

		seedRng(parseInt(window.location.href.slice(seedIndex)));
	}
	else {
		let generatedSeed = Math.floor(Math.random() * 1000000000);

		// NOTE (this performs a redirect)

		window.location.replace(window.location.href + "?seed=" + generatedSeed);
		return;
	}

	// Initialize new card button

	document.getElementById("btnNewCard").addEventListener("click", function () {
		let locationSubstring = window.location.href;
		if (window.location.href.includes("?seed=")) {
			locationSubstring = locationSubstring.substring(0, window.location.href.indexOf("?seed="));
		}
		
		let generatedSeed = Math.floor(Math.random() * 1000000000);

		// NOTE (this performs a redirect)

		window.location.replace(locationSubstring + "?seed=" + generatedSeed);
		return;
	});

	// Initialize row/column headers

	function cellsFromHeaderId(headerId) {
		let cells = [];

		if (headerId === "tlbr") {
			cells.push(document.getElementById("0_0"));
			cells.push(document.getElementById("1_1"));
			cells.push(document.getElementById("2_2"));
			cells.push(document.getElementById("3_3"));
			cells.push(document.getElementById("4_4"));
		}
		else if (headerId === "bltr") {
			cells.push(document.getElementById("4_0"));
			cells.push(document.getElementById("3_1"));
			cells.push(document.getElementById("2_2"));
			cells.push(document.getElementById("1_3"));
			cells.push(document.getElementById("0_4"));
		}
		else if (headerId.startsWith("col")) {
			let col = headerId.slice(-1);
			col = parseInt(col) - 1;

			for (let i = 0; i < 5; i++) {
				cells.push(document.getElementById(i + "_" + col));
			}
		}
		else if (headerId.startsWith("row")) {
			let row = headerId.slice(-1);
			row = parseInt(row) - 1;

			for (let i = 0; i < 5; i++) {
				cells.push(document.getElementById(row + "_" + i));
			}
		}
		else {
			assert(false);
		}

		return cells;
	}

	let headerSelected = null;
	let headers = document.getElementsByTagName("th");
	for (let i = 0; i < headers.length; i++) {
		let cells = cellsFromHeaderId(headers[i].id);

		headers[i].addEventListener("mouseenter", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.add("fakeHover");
			}
		});

		headers[i].addEventListener("mouseout", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.remove("fakeHover");
				cells[i].classList.remove("preselected");
			}
		});

		headers[i].addEventListener("mousedown", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.add("preselected");
			}
		});

		headers[i].addEventListener("mouseup", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.remove("preselected");
			}
		});

		headers[i].addEventListener("click", function () {
			if (headerSelected !== null) {

				assert(headerSelected.classList.contains("selected"));
				headerSelected.classList.remove("selected");

				let cellsPrev = cellsFromHeaderId(headerSelected.id);
				for (let i = 0; i < cellsPrev.length; i++) {
					cellsPrev[i].classList.remove("selected");
				}
			}

			if (headerSelected === this) {
				headerSelected = null;
			}
			else {
				headerSelected = this;

				assert(!headerSelected.classList.contains("selected"));
				headerSelected.classList.add("selected");

				for (let i = 0; i < cells.length; i++) {
					cells[i].classList.add("selected");
				}
			}
		});
	}

	// Init click listener for goals

	let goalCells = [];
	for (let i = 0; i < 5; i++) {
		goalCells.push([]);

		for (let j = 0; j < 5; j++) {
			let cell = document.getElementById(i + "_" + j);
			cell.addEventListener("click", function () {
				if (this.classList.contains("greenCell")) {
					this.classList.add("redCell");
					this.classList.remove("greenCell");
				}
				else if (this.classList.contains("redCell")) {
					this.classList.remove("redCell");
				}
				else {
					this.classList.add("greenCell");
				}
			});
		}
	}
	
	// Recalculate layout on window resize

	addEventListener("resize", initLayout);
}

function chooseGoals() {

	// Init extra fields on goals

	for (let i = 0; i < manifest.goals.length; i++) {
		manifest.goals[i].isInBoard = false;
	}

	// Shuffle goals in manifest

	for (let i = 0; i < manifest.goals.length; i++) {
		let iNew = i + rngInt(manifest.goals.length - i);

		var tmp = manifest.goals[i];
		manifest.goals[i] = manifest.goals[iNew];
		manifest.goals[iNew] = tmp;
	}

	let iGoalNext = 0;
	function nextGoal() {
		while (manifest.goals[iGoalNext].isInBoard) {
			iGoalNext += 1;
			iGoalNext %= manifest.goals.length;
		}

		var result = manifest.goals[iGoalNext];
		iGoalNext += 1;
		iGoalNext %= manifest.goals.length;
		return result;
	}

	// Set up 5x5 buffer

	let goalGrid = [];
	for (let i = 0; i < 5; i++) {
		goalGrid.push([]);
		for (let j = 0; j < 5; j++) {
			goalGrid[i].push(null);
		}
	}

	function setGoal(row, col, goal) {
		let oldGoal = goalGrid[row][col];
		if (oldGoal !== null) {
			oldGoal.isInBoard = false;
		}

		goalGrid[row][col] = goal;
		goal.isInBoard = true;
	}

	// Pick random goals for each cell

	for (let i = 0; i < 5; i++) {
		for (let j = 0; j < 5; j++) {
			setGoal(i, j, nextGoal());
		}
	}

	function goalsFromHeaderId(headerId) {
		let goals = [];

		if (headerId === "tlbr") {
			goals.push(goalGrid[0][0]);
			goals.push(goalGrid[1][1]);
			goals.push(goalGrid[2][2]);
			goals.push(goalGrid[3][3]);
			goals.push(goalGrid[4][4]);
		}
		else if (headerId === "bltr") {
			goals.push(goalGrid[4][0]);
			goals.push(goalGrid[3][1]);
			goals.push(goalGrid[2][2]);
			goals.push(goalGrid[1][3]);
			goals.push(goalGrid[0][4]);
		}
		else if (headerId.startsWith("col")) {
			let col = headerId.slice(-1);
			col = parseInt(col) - 1;

			for (let i = 0; i < 5; i++) {
				goals.push(goalGrid[i][col]);
			}
		}
		else if (headerId.startsWith("row")) {
			let row = headerId.slice(-1);
			row = parseInt(row) - 1;

			for (let i = 0; i < 5; i++) {
				goals.push(goalGrid[row][i]);
			}
		}
		else {
			assert(false);
		}

		return goals;
	}

	function evaluateGoals(goals) {
		function getSynergy(goal0, goal1) {
			// NOTE (andrew) You only get the biggest synergy. You don't get to double dip multiple synergies, as there are certain tags
			//	that always imply other less specific tags. It is very likely that both tags synergize with similar things, and it would
			//	be silly to give you the synergy twice. Just choose the one that is biggest. The tag synergies are authored with this in mind.

			// TODO: Go thru all tags pairwise and choose the biggest synergy to return.

			return 0;
		}

		assert(goals.length === 5);

		let result = {
			rawScore: 0,
			totalSynergy: 0,
			finalScore: 0,
			tagViolations: [],
			nearTagViolations: []		// Tags that are 1 away from exceeding limit
		}

		for (let iGoal = 0; iGoal < 5; iGoal++) {
			let goal = goals[iGoal];
			let mpTagidGoals = Array(manifest.tags.length).fill({ goals: []});

			// Check max tag per header

			for (let iTag = 0; iTag < goal.tags.length; iTag++) {
				let tagid = goal.tags[iTag];
				let maxTagPerHeader = manifest.tags[tagid].maxPerRow;

				mpTagidGoals[tagid].goals.push(goal);

				if (mpTagidGoals[tagid].length >= maxTagPerHeader) {

					let ntvExisting = nearTagViolations.find(v => v.tag == tagid);
					let tvExisting = tagViolations.find(v => v.tag == tagid);

					if (mpTagidGoals[tagid].length === maxTagPerHeader) {
						assert(!ntvExisting);
						assert(!tvExisting);

						nearTagViolations.push({ tag: tagid, goals: mpTagidGoals[tagid].goals });
					}
					else if (mpTagidGoals[tagid].length === maxTagPerHeader + 1) {
						assert(ntvExisting);
						assert(!tvExisting);

						nearTagViolations.remove(ntvExisting);
						tagViolations.push({ tag: tagid, goals: mpTagidGoals[tagid].goals });
					}
					else {
						assert(!ntvExisting);
						assert(tvExisting);

						tvExisting.goals.push(goal);
					}
				}
			}

			// Check synergy

			for (let iGoalOther = iGoal + 1; iGoalOther < 5; iGoalOther++) {
				let goalOther = goals[iGoalOther];
				result.totalSynergy += getSynergy(goal, goalOther);
			}

			result.rawScore += goal.score;
		}

		result.finalScore = result.rawScore - result.totalSynergy;
	}
	

	let stopMutating = false;
	let iterations = 0;
	let iterationsMax = 50;
	let headers = ["row1", "row2", "row3", "row4", "row5", "col1", "col2", "col3", "col4", "col5", "tlbr", "bltr"];

	// TODO: Easy, medium, hard difficulties
	// NOTE (andrew) sqrt is intended to dampen the volatility a little bit. It is definitely a knob that can be tweaked.

	let avgScorePostSynergy = manifest.goalScoreAvg - manifest.pairwiseGoalSynergyAvg;
	let headerScoreMin = avgScorePostSynergy * 5 - Math.sqrt(5 * manifest.goalScoreStddev);
	let headerScoreIdeal = avgScorePostSynergy * 5;
	let headerScoreMax = avgScorePostSynergy * 5 + Math.sqrt(5 * manifest.goalScoreStddev);

	while (iterations < iterationsMax && !stopMutating) {
		stopMutating = true;

		for (let i = 0; i < headers.length; i++) {
			let header = headers[i];
			let goals = getGoalsForHeader(header);
			let evaluation = evaluateGoals(goals);

			let dScoreIdeal = headerScoreIdeal - evaluation.total
			let cNeedReplaceDueToTag = 0;	// TODO: compute. Keep in mind that multiple violations could be fixed by a single replace if it has multiple tags...

			let dScorePerReplace = dScoreIdeal / cNeedReplaceDueToTag;	// TODO: watch for divide by zero here. Need different strategy to nudge difficulty up/down if the tags are fine but difficulty isn't.

			// TODO: If tag violations, try to replace them with better tags (use neartagviolations to guide you)
			// BB (andrew) Maybe the result value should just have a full index of tagid -> goals, sorted by count. That way we can keep the value updated as we swap
			//	potentially multiple goals out.
		}
	}

	if (!stopMutating)
	{
		assert(iterations == iterationsMax);
		console.log("Reached max iteration count while mutating board");
	}

	return goalGrid;
}

function buildBoardHtml(goals) {
	for (let i = 0; i < 5; i++) {
		for (let j = 0; j < 5; j++) {
			let cell = document.getElementById(i + "_" + j);
			let goal = goals[i][j];

			cell.innerHTML = htmlFromGoal(goal);
		}
	}
}

initLayout();
initHandlers();
let goals = chooseGoals();
buildBoardHtml(goals);

document.getElementById("panelMain").style.visibility = "visible";